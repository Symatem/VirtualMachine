#include "Ontology.hpp"

struct Context { // TODO: : public Storage {
    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode;
    Symbol nextSymbol;
    typedef std::map<Symbol, std::unique_ptr<SymbolObject>>::iterator TopIter;
    std::map<Symbol, std::unique_ptr<SymbolObject>> topIndex;
    struct BlobIndexCompare {
        bool operator()(const SymbolObject* a, const SymbolObject* b) const {
            return a->compareBlob(*b) < 0;
        }
    };
    std::map<SymbolObject*, Symbol, BlobIndexCompare> blobIndex;

    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        auto& subIndex = topIter->second->subIndices[0];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        auto gammaIter = betaIter->second.find(triple.pos[2]);
        if(gammaIter == betaIter->second.end())
            return 0;
        if(callback) callback();
        return 1;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        auto& subIndex = topIter->second->subIndices[index];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        if(callback)
            for(auto gamma : betaIter->second) {
                triple.pos[2] = gamma;
                callback();
            }
        return betaIter->second.size();
    }

    ArchitectureType searchGVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        auto& subIndex = topIter->second->subIndices[index];
        ArchitectureType count = 0;
        for(auto& beta : subIndex) {
            count += beta.second.size();
            if(callback) {
                triple.pos[1] = beta.first;
                for(auto gamma : beta.second) {
                    triple.pos[2] = gamma;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        auto& subIndex = topIter->second->subIndices[index];
        std::set<Symbol> result; // TODO: Replace me!
        for(auto& beta : subIndex)
            result.insert(beta.second.begin(), beta.second.end());
        if(callback)
            for(auto gamma : result) {
                triple.pos[2] = gamma;
                callback();
            }
        return result.size();
    }

    ArchitectureType searchGVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        auto& subIndex = topIter->second->subIndices[index];
        if(callback)
            for(auto& beta : subIndex) {
                triple.pos[1] = beta.first;
                callback();
            }
        return subIndex.size();
    }

    ArchitectureType searchVII(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        if(callback)
            for(auto& alpha : topIndex) {
                triple.pos[0] = alpha.first;
                callback();
            }
        return topIndex.size();
    }

    ArchitectureType searchVVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : topIndex) {
            auto& subIndex = alpha.second->subIndices[index];
            count += subIndex.size();
            if(callback) {
                triple.pos[0] = alpha.first;
                for(auto& beta : subIndex) {
                    triple.pos[1] = beta.first;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchVVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : topIndex) {
            triple.pos[0] = alpha.first;
            for(auto& beta : alpha.second->subIndices[0]) {
                count += beta.second.size();
                if(callback) {
                    triple.pos[1] = beta.first;
                    for(auto gamma : beta.second) {
                        triple.pos[2] = gamma;
                        callback();
                    }
                }
            }
        }
        return count;
    }

    ArchitectureType query(ArchitectureType mode, Triple triple, std::function<void(Triple, ArchitectureType)> callback = nullptr) {
        struct QueryMethod {
            uint8_t index, pos, size;
            ArchitectureType(Context::*function)(ArchitectureType, Triple&, std::function<void()>);
        };

        const QueryMethod lookup[] = {
            {EAV, 0, 0, &Context::searchGGG},
            {AVE, 2, 1, &Context::searchGGV},
            {AVE, 0, 0, nullptr},
            {VEA, 2, 1, &Context::searchGGV},
            {VEA, 1, 2, &Context::searchGVV},
            {VAE, 1, 1, &Context::searchGVI},
            {VEA, 0, 0, nullptr},
            {VEA, 1, 1, &Context::searchGVI},
            {VEA, 0, 0, nullptr},
            {EAV, 2, 1, &Context::searchGGV},
            {AVE, 1, 2, &Context::searchGVV},
            {AVE, 1, 1, &Context::searchGVI},
            {EAV, 1, 2, &Context::searchGVV},
            {EAV, 0, 3, &Context::searchVVV},
            {AVE, 0, 2, &Context::searchVVI},
            {EVA, 1, 1, &Context::searchGVI},
            {VEA, 0, 2, &Context::searchVVI},
            {VEA, 0, 1, &Context::searchVII},
            {EAV, 0, 0, nullptr},
            {AEV, 1, 1, &Context::searchGVI},
            {AVE, 0, 0, nullptr},
            {EAV, 1, 1, &Context::searchGVI},
            {EAV, 0, 2, &Context::searchVVI},
            {AVE, 0, 1, &Context::searchVII},
            {EAV, 0, 0, nullptr},
            {EAV, 0, 1, &Context::searchVII},
            {EAV, 0, 0, nullptr}
        };

        if(mode >= sizeof(lookup)/sizeof(QueryMethod))
            throw Exception("Invalid Mode Value");
        QueryMethod method = lookup[mode];
        if(method.function == nullptr)
            throw Exception("Invalid Mode Value");

        std::function<void()> handleNext = [&]() {
            Triple result;
            for(ArchitectureType i = 0; i < method.size; ++i)
                result.pos[i] = triple.pos[method.pos+i];
            callback(result, method.size);
        };

        if(indexMode == MonoIndex) {
            if(method.index != EAV) {
                method.index = EAV;
                method.function = &Context::searchVVV;
                handleNext = [&]() {
                    ArchitectureType index = 0;
                    if(mode%3 == 1) ++index;
                    if(mode%9 >= 3 && mode%9 < 6) {
                        triple.pos[index] = triple.pos[1];
                        ++index;
                    }
                    if(mode >= 9 && mode < 18)
                        triple.pos[index] = triple.pos[2];
                    callback(triple, method.size);
                };
            }
        } else if(indexMode == Context::TriIndex && method.index >= 3) {
            method.index -= 3;
            method.pos = 2;
            method.function = &Context::searchGIV;
        }

        triple = triple.reordered(method.index);
        if(!callback) handleNext = nullptr;
        return (this->*method.function)(method.index, triple, handleNext);
    }

    bool valueSetCountIs(Symbol entity, Symbol attribute, ArchitectureType size) {
        return query(9, {entity, attribute, PreDef_Void}) == size;
    }

    bool tripleExists(Triple triple) {
        return query(0, triple) == 1;
    }

    bool link(Triple triple, bool allowFailure = false) {
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            auto topIter = topIndex.find(triple.pos[i]);
            if(topIter == topIndex.end())
                topIter = SymbolFactory(triple.pos[i]);
            if(!topIter->second->link(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3])) {
                if(allowFailure)
                    return false;
                throw Exception("Already linked", {
                    {PreDef_Entity, triple.entity},
                    {PreDef_Attribute, triple.attribute},
                    {PreDef_Value, triple.value}
                });
            }
        }
        return true;
    }

    bool unlink(std::set<Triple> triples, bool allowFailure = false, std::set<Symbol> symbols = {}) {
        std::set<Symbol> dirty; // TODO: Replace me!
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(auto& triple : triples) {
            for(ArchitectureType i = 0; i < indexCount; ++i) {
                dirty.insert(triple.pos[i]);
                if(symbols.find(triple.pos[i]) != symbols.end())
                    continue;
                auto topIter = topIndex.find(triple.pos[i]);
                if(topIter == topIndex.end() ||
                   !topIter->second->unlink(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3])) {
                    if(allowFailure)
                        return false;
                    throw Exception("Already unlinked", {
                        {PreDef_Entity, triple.entity},
                        {PreDef_Attribute, triple.attribute},
                        {PreDef_Value, triple.value}
                    });
                }
            }
            if(triple.pos[1] == PreDef_BlobType)
                unindexBlob(triple.pos[0]);
        }
        for(auto alpha : dirty) {
            auto topIter = topIndex.find(alpha);
            bool empty = true;
            for(ArchitectureType i = 0; i < indexCount; ++i)
                if(!topIter->second->subIndices[i].empty()) {
                    empty = false;
                    break;
                }
            if(empty)
                topIndex.erase(topIter);
        }
        for(auto alpha : symbols)
            topIndex.erase(alpha);
        return true;
    }

    bool unlink(Symbol alpha, Symbol beta, bool allowFailure = false) {
        std::set<Triple> triples; // TODO: Replace me!
        query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            triples.insert({alpha, beta, result.pos[0]});
        });
        if(triples.empty())
            return true;
        return unlink(triples, allowFailure);
    }

    bool unlink(Triple triple, bool allowFailure = false) {
        return unlink(std::set<Triple>{triple}, allowFailure);
    }

    bool destroy(Symbol alpha, bool allowFailure = false) {
        std::set<Triple> triples; // TODO: Replace me!
        auto topIter = topIndex.find(alpha);
        if(topIter == topIndex.end()) {
            if(allowFailure)
                return false;
            throw Exception("Already destroyed", {
                {PreDef_Entity, alpha}
            });
        }
        for(ArchitectureType i = EAV; i <= VEA; ++i)
            for(auto& beta : topIter->second->subIndices[i])
                for(auto gamma : beta.second)
                    triples.insert(Triple(alpha, beta.first, gamma).normalized(i));
        unlink(triples, false, {alpha});
        return true;
    }

    void scrutinizeExistence(Symbol symbol) {
        std::set<Symbol> symbols; // TODO: Replace me!
        symbols.insert(symbol);
        while(!symbols.empty()) {
            symbol = *symbols.begin();
            symbols.erase(symbols.begin());
            if(topIndex.find(symbol) == topIndex.end() ||
               query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
                continue;
            query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
                symbols.insert(result.pos[0]);
            });
            destroy(symbol);
        }
    }

    void setSolitary(Triple triple) {
        bool toLink = true;
        std::set<Triple> triples; // TODO: Replace me!
        query(9, triple, [&](Triple result, ArchitectureType) {
            if(triple.pos[2] == result.pos[0])
                toLink = false;
            else
                triples.insert({triple.pos[0], triple.pos[1], result.pos[0]});
        });
        if(toLink)
            link(triple);
        if(!triples.empty())
            unlink(triples);
    }

    bool getUncertain(Symbol alpha, Symbol beta, Symbol& gamma) {
        if(query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            gamma = result.pos[0];
        }) != 1)
            return false;
        return true;
    }

    Symbol getGuaranteed(Symbol entity, Symbol attribute) {
        Symbol value;
        if(!getUncertain(entity, attribute, value))
            throw Exception("Nonexistent or Ambiguous", {
                {PreDef_Entity, entity},
                {PreDef_Attribute, attribute}
            });
        return value;
    }

    SymbolObject* getSymbolObject(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        if(topIter == topIndex.end())
            throw Exception("Symbol is Nonexistent");
        return topIter->second.get();
    }

    TopIter SymbolFactory(Symbol symbol) {
        return topIndex.insert(std::make_pair(symbol, std::unique_ptr<SymbolObject>(new SymbolObject()))).first;
    }

    Symbol create(std::set<std::pair<Symbol, Symbol>> links = {}) {
        Symbol symbol = nextSymbol++;
        SymbolFactory(symbol);
        for(auto l : links)
            link({symbol, l.first, l.second});
        return symbol;
    }

    Symbol createFromData(const char* src, ArchitectureType len) {
        Symbol symbol = create({{PreDef_BlobType, PreDef_Text}});
        SymbolObject* symbolObject = getSymbolObject(symbol);
        symbolObject->allocateBlob(len*8);
        auto dst = reinterpret_cast<uint8_t*>(symbolObject->blobData.get());
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
        return symbol;
    }

    Symbol createFromData(const char* src) {
        ArchitectureType len = 0;
        while(src[len])
            ++len;
        return createFromData(src, len);
    }

    template<typename DataType>
    Symbol createFromData(DataType src) {
        Symbol blobType;
        if(typeid(DataType) == typeid(uint64_t))
            blobType = PreDef_Natural;
        else if(typeid(DataType) == typeid(int64_t))
            blobType = PreDef_Integer;
        else if(typeid(DataType) == typeid(double))
            blobType = PreDef_Float;
        else
            throw Exception("Unknown Data Type");
        Symbol symbol = create({{PreDef_BlobType, blobType}});
        getSymbolObject(symbol)->overwriteBlob(src);
        return symbol;
    }

    // TODO: Needs to be called at every blob mutation
    bool unindexBlob(Symbol symbol) {
        auto iter = blobIndex.find(getSymbolObject(symbol));
        if(iter == blobIndex.end() || iter->second != symbol)
            return false;
        blobIndex.erase(iter);
        return true;
    }

    Context() :nextSymbol(0), indexMode(HexaIndex) {
        while(nextSymbol < sizeof(PreDefSymbols)/sizeof(void*)) {
            Symbol symbol = createFromData(PreDefSymbols[nextSymbol]);
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbol});
            blobIndex.insert(std::make_pair(getSymbolObject(symbol), symbol));
        }
        Symbol ArchitectureSizeSymbol = createFromData(ArchitectureSize);
        link({PreDef_RunTimeEnvironment, PreDef_Holds, ArchitectureSizeSymbol});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, ArchitectureSizeSymbol});
    }
};
