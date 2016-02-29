#include "Thread.hpp"

const char* HRLRawBegin = "raw:";

struct Serialize {
    Thread& thread;
    Symbol symbol;
    SymbolObject* symbolObject;
    ArchitectureType blobSize;

    bool isEmpty() const {
        return (blobSize == 0);
    }

    Symbol finalizeSymbol() {
        symbolObject->reallocateBlob(blobSize);
        return symbol;
    }

    void put(uint8_t data) {
        ArchitectureType nextBlobSize = blobSize+8;
        if(nextBlobSize > symbolObject->blobSize)
            symbolObject->reallocateBlob(std::max(nextBlobSize, symbolObject->blobSize*2));
        reinterpret_cast<uint8_t*>(symbolObject->blobData.get())[blobSize/8] = data;
        blobSize = nextBlobSize;
    }

    template<typename NumberType>
    void serializeNumber(NumberType number) {
        // TODO Last digits of float numbers are incorrect (rounding error)

        const NumberType base = 10;
        if(number == 0) {
            put('0');
            return;
        }
        if(number < 0) {
            put('-');
            number *= -1;
        }
        NumberType mask = 1;
        while(mask <= number)
            mask *= base;
        while(mask > 0.001) {
            if(mask == 1 && number > 0) // TODO
                put('.');
            mask /= base;
            if(mask == 0)
                break;
            NumberType digit = number/mask;
            digit = static_cast<int64_t>(digit); // TODO
            number -= digit*mask;
            put('0'+digit);
        }
    }

    Serialize(Thread& _thread, Symbol _symbol) :thread(_thread), symbol(_symbol), blobSize(0) {
        symbolObject = Ontology::getSymbolObject(_symbol);
        thread.setSolitary({symbol, PreDef_BlobType, PreDef_Text});
    }

    Serialize(Thread& _thread) :Serialize(_thread, Ontology::create()) { }

    void serializeBlob(Symbol symbol) {
        auto srcSymbolObject = Ontology::getSymbolObject(symbol);
        auto src = reinterpret_cast<const uint8_t*>(srcSymbolObject->blobData.get());
        if(srcSymbolObject->blobSize) {
            Symbol type = PreDef_Void;
            thread.getUncertain(symbol, PreDef_BlobType, type);
            switch(type) {
                case PreDef_Text: {
                    ArchitectureType len = srcSymbolObject->blobSize/8;
                    bool spaces = false;
                    for(ArchitectureType i = 0; i < len; ++i)
                        if(src[i] == ' ' || src[i] == '\t' || src[i] == '\n') {
                            spaces = true;
                            break;
                        }
                    if(spaces)
                        put('"');
                    for(ArchitectureType i = 0; i < len; ++i)
                        put(src[i]);
                    if(spaces)
                        put('"');
                }   break;
                case PreDef_Natural:
                    serializeNumber(thread.accessBlobAs<uint64_t>(srcSymbolObject));
                    break;
                case PreDef_Integer:
                    serializeNumber(thread.accessBlobAs<int64_t>(srcSymbolObject));
                    break;
                case PreDef_Float:
                    serializeNumber(thread.accessBlobAs<double>(srcSymbolObject));
                    break;
                default: {
                    for(ArchitectureType i = 0; i < strlen(HRLRawBegin); ++i)
                        put(HRLRawBegin[i]);
                    ArchitectureType len = (srcSymbolObject->blobSize+3)/4;
                    for(ArchitectureType i = 0; i < len; ++i) {
                        uint8_t nibble = (src[i/2]>>((i%2)*4))&0x0F;
                        if(nibble < 0xA)
                            put('0'+nibble);
                        else
                            put('A'+nibble-0xA);
                    }
                }   break;
            }
        } else {
            put('#');
            serializeNumber(symbol);
        }
    }

    void serializeEntity(Symbol entity, std::function<Symbol(Symbol)> followCallback = nullptr) {
        while(true) {
            Symbol followAttribute, followEntity = PreDef_Void;
            if(followCallback)
                followAttribute = followCallback(entity);

            put('(');
            put('\n');
            put('\t');
            serializeBlob(entity);
            put(';');
            put('\n');

            thread.query(21, {entity, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                if(followCallback && result.pos[0] == followAttribute) {
                    thread.getUncertain(entity, followAttribute, followEntity);
                    return;
                }
                put('\t');
                serializeBlob(result.pos[0]);
                thread.query(9, {entity, result.pos[0], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                    put(' ');
                    serializeBlob(resultB.pos[0]);
                });
                put(';');
                put('\n');
            });

            if(!followCallback || followEntity == PreDef_Void) {
                put(')');
                return;
            }

            put('\t');
            serializeBlob(followAttribute);
            entity = followEntity;
            put('\n');
            put(')');
            put(' ');
        }
    }
};
