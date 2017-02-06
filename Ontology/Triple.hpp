#include <Ontology/Containers.hpp>

#define Wrapper(token) token##Symbol
enum PreDefinedSymbols {
#include <Ontology/Symbols.hpp>
};
#undef Wrapper

#define Wrapper(token) #token
const char* PreDefinedSymbols[] = {
#include <Ontology/Symbols.hpp>
};
#undef Wrapper

const Symbol preDefinedSymbolsCount = sizeof(PreDefinedSymbols)/sizeof(void*);

#define forEachSubIndex \
    NativeNaturalType indexCount = (indexMode == MonoIndex) ? 1 : 3; \
    for(NativeNaturalType subIndex = 0; subIndex < indexCount; ++subIndex)

struct Triple {
    Symbol pos[3];

    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :pos{_entity, _attribute, _value} {}

    Triple forwardIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
        return {subIndices[subIndex], pos[(subIndex+1)%3], pos[(subIndex+2)%3]};
    }

    Triple invertedIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
        return {subIndices[subIndex+3], pos[(subIndex+2)%3], pos[(subIndex+1)%3]};
    }

    Triple reordered(NativeNaturalType subIndex) {
        static const NativeNaturalType
            alpha[] = {0, 1, 2, 0, 1, 2},
             beta[] = {1, 2, 0, 2, 0, 1},
            gamma[] = {2, 0, 1, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }

    Triple normalized(NativeNaturalType subIndex) {
        static const NativeNaturalType
            alpha[] = {0, 2, 1, 0, 1, 2},
             beta[] = {1, 0, 2, 2, 0, 1},
            gamma[] = {2, 1, 0, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }

    bool operator==(Triple const& other) {
        return pos[0] == other.pos[0] && pos[1] == other.pos[1] && pos[2] == other.pos[2];
    }

    bool operator>(Triple const& other) {
        if(pos[2] > other.pos[2])
            return true;
        if(pos[2] < other.pos[2])
            return false;
        if(pos[1] > other.pos[1])
            return true;
        if(pos[1] < other.pos[1])
            return false;
        return pos[0] > other.pos[0];
    }
};

enum IndexType {
    EAV = 0, AVE = 1, VEA = 2,
    EVA = 3, AEV = 4, VAE = 5
};

enum IndexMode {
    MonoIndex = 1,
    TriIndex = 3,
    HexaIndex = 6
} indexMode = HexaIndex;

enum QueryMode {
    Match,
    Varying,
    Ignore
};

enum QueryMask {
    MMM, VMM, IMM, MVM, VVM, IVM, MIM, VIM, IIM,
    MMV, VMV, IMV, MVV, VVV, IVV, MIV, VIV, IIV,
    MMI, VMI, IMI, MVI, VVI, IVI, MII, VII, III
};

BlobSet<false, Symbol, Symbol[6]> tripleIndex;
BlobIndex<false> blobIndex;

bool linkInSubIndex(Triple triple) {
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = triple.pos[0];
    NativeNaturalType betaIndex;
    BlobSet<false, Symbol> gamma;
    if(beta.find(triple.pos[1], betaIndex))
        gamma.symbol = beta.readElementAt(betaIndex).value;
    else {
        gamma.symbol = createSymbol();
        beta.insert(betaIndex, {triple.pos[1], gamma.symbol});
    }
    return gamma.insertElement(triple.pos[2]);
}

bool linkTriplePartial(Triple triple, NativeNaturalType subIndex) {
    NativeNaturalType alphaIndex;
    Pair<Symbol, Symbol[6]> element;
    if(!tripleIndex.find(triple.pos[subIndex], alphaIndex)) {
        element.key = triple.pos[subIndex];
        for(NativeNaturalType i = 0; i < 6; ++i)
            element.value[i] = createSymbol();
        tripleIndex.insert(alphaIndex, element);
    } else
        element = tripleIndex.readElementAt(alphaIndex);
    if(!linkInSubIndex(triple.forwardIndex(element.value, subIndex)))
        return false;
    if(indexMode == HexaIndex)
        assert(linkInSubIndex(triple.invertedIndex(element.value, subIndex)));
    return true;
}

bool link(Triple triple) {
    forEachSubIndex
        if(!linkTriplePartial(triple, subIndex))
            return false;
    if(triple.pos[1] == BlobTypeSymbol)
        modifiedBlob(triple.pos[0]);
    return true;
}

NativeNaturalType searchMMM(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType alphaIndex, betaIndex, gammaIndex;
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    if(!beta.find(triple.pos[1], betaIndex))
        return 0;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(!gamma.find(triple.pos[2], gammaIndex))
        return 0;
    if(callback)
        callback(triple);
    return 1;
}

NativeNaturalType searchMMV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType alphaIndex, betaIndex;
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    if(!beta.find(triple.pos[1], betaIndex))
        return 0;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(callback)
        gamma.iterateKeys([&](Symbol gammaResult) {
            triple.pos[2] = gammaResult;
            callback(triple.normalized(subIndex));
        });
    return gamma.size();
}

NativeNaturalType searchMVV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType alphaIndex, count = 0;
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
        BlobSet<false, Symbol> gamma;
        gamma.symbol = betaResult.value;
        if(callback) {
            triple.pos[1] = betaResult.key;
            gamma.iterateKeys([&](Symbol gammaResult) {
                triple.pos[2] = gammaResult;
                callback(triple.normalized(subIndex));
            });
        }
        count += gamma.size();
    });
    return count;
}

NativeNaturalType searchMIV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType alphaIndex;
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<true, Symbol> result;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
        BlobSet<false, Symbol> gamma;
        gamma.symbol = betaResult.value;
        gamma.iterateKeys([&](Symbol gammaResult) {
            result.insertElement(gammaResult);
        });
    });
    if(callback)
        result.iterate([&](Symbol gamma) {
            triple.pos[2] = gamma;
            callback(triple.normalized(subIndex));
        });
    return result.size();
}

NativeNaturalType searchMVI(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType alphaIndex;
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    if(callback)
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            triple.pos[1] = betaResult.key;
            callback(triple.normalized(subIndex));
        });
    return beta.size();
}

NativeNaturalType searchVII(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    if(callback)
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            triple.pos[0] = alphaResult.key;
            callback(triple.normalized(subIndex));
        });
    return tripleIndex.size();
}

NativeNaturalType searchVVI(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType count = 0;
    tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = alphaResult.value[subIndex];
        if(callback) {
            triple.pos[0] = alphaResult.key;
            beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
                triple.pos[1] = betaResult.key;
                callback(triple.normalized(subIndex));
            });
        }
        count += beta.size();
    });
    return count;
}

NativeNaturalType searchVVV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType count = 0;
    tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
        triple.pos[0] = alphaResult.key;
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = alphaResult.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            BlobSet<false, Symbol> gamma;
            gamma.symbol = betaResult.value;
            if(callback) {
                triple.pos[1] = betaResult.key;
                gamma.iterateKeys([&](Symbol gammaResult) {
                    triple.pos[2] = gammaResult;
                    callback(triple);
                });
            }
            count += gamma.size();
        });
    });
    return count;
}

NativeNaturalType query(QueryMask mask, Triple triple = {VoidSymbol, VoidSymbol, VoidSymbol}, Closure<void(Triple)> callback = nullptr) {
    struct QueryMethod {
        NativeNaturalType subIndex, offset, size;
        NativeNaturalType(*function)(NativeNaturalType, Triple, Closure<void(Triple)>);
    };
    const QueryMethod lookup[] = {
        {EAV, 0, 0, &searchMMM},
        {AVE, 2, 1, &searchMMV},
        {AVE, 0, 0, nullptr},
        {VEA, 2, 1, &searchMMV},
        {VEA, 1, 2, &searchMVV},
        {VAE, 1, 1, &searchMVI},
        {VEA, 0, 0, nullptr},
        {VEA, 1, 1, &searchMVI},
        {VEA, 0, 0, nullptr},
        {EAV, 2, 1, &searchMMV},
        {AVE, 1, 2, &searchMVV},
        {AVE, 1, 1, &searchMVI},
        {EAV, 1, 2, &searchMVV},
        {EAV, 0, 3, &searchVVV},
        {AVE, 0, 2, &searchVVI},
        {EVA, 1, 1, &searchMVI},
        {VEA, 0, 2, &searchVVI},
        {VEA, 0, 1, &searchVII},
        {EAV, 0, 0, nullptr},
        {AEV, 1, 1, &searchMVI},
        {AVE, 0, 0, nullptr},
        {EAV, 1, 1, &searchMVI},
        {EAV, 0, 2, &searchVVI},
        {AVE, 0, 1, &searchVII},
        {EAV, 0, 0, nullptr},
        {EAV, 0, 1, &searchVII},
        {EAV, 0, 0, nullptr}
    };
    assert(mask < sizeof(lookup)/sizeof(QueryMethod));
    QueryMethod method = lookup[mask];
    if(method.function == nullptr)
        return 0;
    Triple match = triple;
    BlobSet<true, Triple> resultSet;
    auto monoIndexLambda = [&](Triple result) {
        static const NativeNaturalType maskDivisor[] = {1, 3, 9};
        for(NativeNaturalType i = 0; i < 3; ++i) {
            NativeNaturalType mode = (mask/maskDivisor[i])%3;
            if(mode == Ignore)
                result.pos[i] = VoidSymbol;
            else if(mode == Match && match.pos[i] != result.pos[i])
                return;
        }
        if(resultSet.insertElement(result) && callback)
            callback(result);
    };
    switch(indexMode) {
        case MonoIndex:
            if(method.subIndex != EAV) {
                method.subIndex = EAV;
                method.function = &searchVVV;
                (*method.function)(method.subIndex, triple, monoIndexLambda);
                return resultSet.size();
            }
        case TriIndex:
            if(method.subIndex >= 3) {
                method.subIndex -= 3;
                method.offset = 2;
                method.function = &searchMIV;
            }
        case HexaIndex:
            return (*method.function)(method.subIndex, triple.reordered(method.subIndex), callback);
    }
}

bool valueCountIs(Symbol entity, Symbol attribute, NativeNaturalType size) {
    return query(MMV, {entity, attribute, VoidSymbol}) == size;
}

bool tripleExists(Triple triple) {
    return query(MMM, triple) == 1;
}

void setIndexMode(IndexMode _indexMode) {
    assert(_indexMode != indexMode);
    if(_indexMode > indexMode) {
        NativeNaturalType indexCount = (_indexMode == MonoIndex) ? 1 : 3;
        searchVVV(EAV, {VoidSymbol, VoidSymbol, VoidSymbol}, [&](Triple result) {
            for(NativeNaturalType subIndex = indexMode; subIndex < indexCount; ++subIndex)
                linkTriplePartial(result, subIndex);
        });
    } else
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            for(NativeNaturalType subIndex = _indexMode; subIndex < indexMode; ++subIndex) {
                BlobSet<false, Symbol, Symbol> beta;
                beta.symbol = alphaResult.value[subIndex];
                beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
                    releaseSymbol(betaResult.value);
                });
                beta.clear();
            }
        });
    indexMode = _indexMode;
}

bool unlinkInSubIndex(Triple triple) {
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = triple.pos[0];
    NativeNaturalType betaIndex, gammaIndex;
    if(!beta.find(triple.pos[1], betaIndex))
        return false;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(!gamma.find(triple.pos[2], gammaIndex))
        return false;
    gamma.erase(gammaIndex);
    if(gamma.empty()) {
        beta.erase(betaIndex);
        releaseSymbol(gamma.symbol);
    }
    return true;
}

bool unlinkWithoutReleasing(Triple triple, bool skipEnabled = false, Symbol skip = VoidSymbol) {
    NativeNaturalType alphaIndex;
    forEachSubIndex {
        if(skipEnabled && triple.pos[subIndex] == skip)
            continue;
        if(!tripleIndex.find(triple.pos[subIndex], alphaIndex))
            return false;
        Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
        if(!unlinkInSubIndex(triple.forwardIndex(element.value, subIndex)))
            return false;
        if(indexMode == HexaIndex)
            assert(unlinkInSubIndex(triple.invertedIndex(element.value, subIndex)));
    }
    if(triple.pos[1] == BlobTypeSymbol)
        modifiedBlob(triple.pos[0]);
    return true;
}

void eraseSymbol(Pair<Symbol, Symbol[6]>& element, NativeNaturalType alphaIndex, Symbol symbol) {
    for(NativeNaturalType subIndex = 0; subIndex < indexMode; ++subIndex) {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            releaseSymbol(betaResult.value);
        });
        releaseSymbol(beta.symbol);
    }
    tripleIndex.erase(alphaIndex);
    releaseSymbol(symbol);
}

void tryToReleaseSymbol(Symbol symbol) {
    NativeNaturalType alphaIndex;
    if(!tripleIndex.find(symbol, alphaIndex))
        return;
    Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
    forEachSubIndex {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        if(!beta.empty())
            return;
    }
    eraseSymbol(element, alphaIndex, symbol);
}

bool unlink(Triple triple) {
    if(!unlinkWithoutReleasing(triple))
        return false;
    for(NativeNaturalType i = 0; i < 3; ++i)
        tryToReleaseSymbol(triple.pos[i]);
    return true;
}

bool unlink(Symbol symbol) {
    NativeNaturalType alphaIndex;
    if(!tripleIndex.find(symbol, alphaIndex)) {
        releaseSymbol(symbol);
        return false;
    }
    Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
    BlobSet<true, Symbol> dirty;
    forEachSubIndex {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            dirty.insertElement(betaResult.key);
            BlobSet<false, Symbol> gamma;
            gamma.symbol = betaResult.value;
            gamma.iterate([&](Symbol gammaResult) {
                dirty.insertElement(gammaResult);
                unlinkWithoutReleasing(Triple(symbol, betaResult.key, gammaResult).normalized(subIndex), true, symbol);
            });
        });
    }
    eraseSymbol(element, alphaIndex, symbol);
    dirty.iterate([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
    return true;
}

void setSolitary(Triple triple, bool linkVoidSymbol = false) {
    BlobSet<true, Symbol> dirty;
    bool toLink = (linkVoidSymbol || triple.pos[2] != VoidSymbol);
    query(MMV, triple, [&](Triple result) {
        if(triple.pos[2] == result.pos[2])
            toLink = false;
        else
            dirty.insertElement(result.pos[2]);
    });
    if(toLink)
        link(triple);
    dirty.iterate([&](Symbol symbol) {
        unlinkWithoutReleasing({triple.pos[0], triple.pos[1], symbol});
    });
    if(!linkVoidSymbol)
        dirty.insertElement(triple.pos[0]);
    dirty.insertElement(triple.pos[1]);
    dirty.iterate([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
}

bool getUncertain(Symbol entity, Symbol attribute, Symbol& value) {
    return (query(MMV, {entity, attribute, VoidSymbol}, [&](Triple result) {
        value = result.pos[2];
    }) == 1);
}

void scrutinizeExistence(Symbol symbol) {
    BlobSet<true, Symbol> symbols;
    symbols.insertElement(symbol);
    while(!symbols.empty()) {
        symbol = symbols.pop_back();
        if(query(VMM, {VoidSymbol, HoldsSymbol, symbol}) > 0)
            continue;
        query(MMV, {symbol, HoldsSymbol, VoidSymbol}, [&](Triple result) {
            symbols.insertElement(result.pos[2]);
        });
        unlink(symbol); // TODO
    }
}

template<typename DataType>
Symbol createFromData(DataType src) {
    static_assert(sizeOfInBits<DataType>::value == architectureSize);
    Symbol blobType = VoidSymbol;
    if(isSame<DataType, NativeNaturalType>::value)
        blobType = NaturalSymbol;
    else if(isSame<DataType, NativeIntegerType>::value)
        blobType = IntegerSymbol;
    else if(isSame<DataType, NativeFloatType>::value)
        blobType = FloatSymbol;
    else
        assert(false);
    Symbol symbol = createSymbol();
    link({symbol, BlobTypeSymbol, blobType});
    Blob(symbol).write(src);
    return symbol;
}

Symbol createFromSlice(Symbol src, NativeNaturalType srcOffset, NativeNaturalType length) {
    Symbol dstSymbol = createSymbol();
    Blob dstBlob(dstSymbol);
    dstBlob.increaseSize(0, length);
    dstBlob.slice(Blob(src), 0, srcOffset, length);
    return dstSymbol;
}

void stringToBlob(const char* src, NativeNaturalType length, Symbol dstSymbol) {
    link({dstSymbol, BlobTypeSymbol, UTF8Symbol});
    Blob dstBlob(dstSymbol);
    dstBlob.increaseSize(0, length*8);
    dstBlob.externalOperate<true>(const_cast<Integer8*>(src), 0, length*8);
    modifiedBlob(dstSymbol);
}

Symbol createFromString(const char* src) {
    Symbol dst = createSymbol();
    stringToBlob(src, strlen(src), dst);
    return dst;
}

bool tryToFillPreDefined(NativeNaturalType additionalSymbols = 0) {
    superPage->version = 0;
    memcpy(superPage->gitRef, gitRef, sizeof(superPage->gitRef));
    superPage->architectureSize = BitMask<NativeNaturalType>::ceilLog2(architectureSize)-1;
    tripleIndex.symbol = TripleIndexSymbol;
    blobIndex.symbol = BlobIndexSymbol;
    if(!tripleIndex.empty())
        return false;
    superPage->symbolsEnd = preDefinedSymbolsCount+additionalSymbols;
    for(Symbol symbol = 0; symbol < preDefinedSymbolsCount-5; ++symbol) {
        const char* str = PreDefinedSymbols[symbol];
        stringToBlob(str, strlen(str), symbol);
        link({RunTimeEnvironmentSymbol, HoldsSymbol, symbol});
        blobIndex.insertElement(symbol);
    }
    Symbol ArchitectureSize = createFromData<NativeNaturalType>(architectureSize);
    link({RunTimeEnvironmentSymbol, HoldsSymbol, ArchitectureSize});
    link({RunTimeEnvironmentSymbol, ArchitectureSizeSymbol, ArchitectureSize});
    return true;
}
