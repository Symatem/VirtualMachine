#include <External/BinaryVariableLength.hpp>

struct StaticHuffmanCodec {
    Blob& blob;
    NativeNaturalType& offset;
    NativeNaturalType symbolCount;

    StaticHuffmanCodec(Blob& _blob, NativeNaturalType& _offset) :blob(_blob), offset(_offset) {}
};

struct StaticHuffmanEncoder : public StaticHuffmanCodec {
    BlobSet<true, Symbol, NativeNaturalType> symbolMap;
    Blob huffmanCodes;

    void countSymbol(Symbol symbol) {
        NativeNaturalType index;
        if(symbolMap.find(symbol, index))
            symbolMap.writeValueAt(index, symbolMap.readValueAt(index)+1);
        else
            symbolMap.insertElement({symbol, 1});
    }

    void encodeSymbol(Symbol symbol) {
        NativeNaturalType index;
        assert(symbolMap.find(symbol, index));
        if(symbolCount < 2)
            return;
        NativeNaturalType begin = symbolMap.readValueAt(index),
                          length = (index+1 < symbolCount) ? symbolMap.readValueAt(index+1)-begin : huffmanCodes.getSize()-begin;
        blob.increaseSize(offset, length);
        blob.interoperation(huffmanCodes, offset, begin, length);
        offset += length;
    }

    void encodeTree() {
        symbolCount = symbolMap.size();
        encodeBvlNatural(blob, offset, symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                encodeBvlNatural(blob, offset, symbolMap.readKeyAt(0));
            return;
        }

        NativeNaturalType index = 0,
                          huffmanChildrenCount = symbolCount-1,
                          huffmanParentsCount = huffmanChildrenCount*2;
        BlobHeap<true, NativeNaturalType, Symbol> symbolHeap;
        symbolHeap.reserve(symbolCount);
        symbolMap.iterateElements([&](Pair<Symbol, NativeNaturalType> pair) {
            symbolHeap.writeElementAt(index, {pair.second, index});
            ++index;
        });
        symbolHeap.build();

        struct HuffmanParentNode {
            NativeNaturalType parent;
            bool bit;
        };
        BlobVector<true, HuffmanParentNode> huffmanParents;
        BlobVector<true, NativeNaturalType> huffmanChildren;
        huffmanParents.reserve(huffmanParentsCount);
        huffmanChildren.reserve(huffmanParentsCount);
        for(NativeNaturalType index = 0; symbolHeap.size() > 1; ++index) {
            auto a = symbolHeap.pop_front(),
                 b = symbolHeap.pop_front();
            NativeNaturalType parent = symbolCount+index;
            huffmanParents.writeElementAt(a.second, {parent, 0});
            huffmanParents.writeElementAt(b.second, {parent, 1});
            huffmanChildren.writeElementAt(index*2, a.second);
            huffmanChildren.writeElementAt(index*2+1, b.second);
            symbolHeap.insertElement({a.first+b.first, parent});
        }
        symbolHeap.clear();

        huffmanCodes.setSize(0);
        NativeNaturalType huffmanCodesOffset = 0;
        for(NativeNaturalType i = 0; i < symbolCount; ++i) {
            NativeNaturalType index = i, length = 0, reserveOffset = 1;
            while(index < huffmanParentsCount) {
                index = huffmanParents.readElementAt(index).parent;
                ++length;
            }
            symbolMap.writeValueAt(i, huffmanCodesOffset);
            huffmanCodes.increaseSize(huffmanCodesOffset, length);
            huffmanCodesOffset += length;
            index = i;
            while(index < huffmanParentsCount) {
                auto node = huffmanParents.readElementAt(index);
                index = node.parent;
                huffmanCodes.externalOperate<true>(&node.bit, huffmanCodesOffset-reserveOffset, 1);
                ++reserveOffset;
            }
        }
        huffmanParents.clear();

        struct HuffmanStackElement {
            NativeNaturalType index;
            Natural8 state;
        };
        BlobVector<true, HuffmanStackElement> stack;
        stack.push_back({huffmanParentsCount, 0});
        while(!stack.empty()) {
            auto element = stack.back();
            switch(element.state) {
                case 0:
                case 1: {
                    NativeNaturalType index = huffmanChildren.readElementAt((element.index-symbolCount)*2+element.state);
                    ++element.state;
                    stack.writeElementAt(stack.size()-1, element);
                    stack.push_back({index, static_cast<Natural8>((index < symbolCount) ? 3 : 0)});
                } break;
                case 2:
                    stack.pop_back();
                    encodeBvlNatural(blob, offset, 0);
                    break;
                case 3: {
                    Symbol symbol = symbolMap.readKeyAt(element.index);
                    encodeBvlNatural(blob, offset, symbol+1);
                    stack.pop_back();
                } break;
            }
        }
        huffmanChildren.clear();
    }

    StaticHuffmanEncoder(Blob& _blob, NativeNaturalType& _offset) :StaticHuffmanCodec(_blob, _offset) {
        huffmanCodes = Blob(createSymbol());
    }

    ~StaticHuffmanEncoder() {
        releaseSymbol(huffmanCodes.symbol);
    }
};

struct StaticHuffmanDecoder : public StaticHuffmanCodec {
    BlobVector<true, Symbol> symbolVector;
    BlobVector<true, NativeNaturalType> huffmanChildren;

    Symbol decodeSymbol() {
        NativeNaturalType index;
        if(symbolCount < 2)
            index = 0;
        else {
            NativeNaturalType mask = 0, bitsLeft = 0;
            index = symbolCount-2;
            while(true) {
                if(bitsLeft == 0) {
                    bitsLeft = min(static_cast<NativeNaturalType>(architectureSize), blob.getSize()-offset);
                    assert(bitsLeft);
                    blob.externalOperate<false>(&mask, offset, bitsLeft);
                    offset += bitsLeft;
                }
                index = huffmanChildren.readElementAt(index*2+(mask&1));
                --bitsLeft;
                if(index < symbolCount)
                    break;
                index -= symbolCount;
                mask >>= 1;
            }
            offset -= bitsLeft;
        }
        return symbolVector.readElementAt(index);
    }

    void decodeTree() {
        symbolCount = decodeBvlNatural(blob, offset);
        symbolVector.reserve(symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                symbolVector.writeElementAt(0, decodeBvlNatural(blob, offset));
            return;
        }
        huffmanChildren.reserve((symbolCount-1)*2);
        BlobVector<true, NativeNaturalType> stack;
        NativeNaturalType symbolIndex = 0, huffmanChildrenIndex = 0;
        while(huffmanChildrenIndex < symbolCount-1) {
            Symbol symbol = decodeBvlNatural(blob, offset);
            if(symbol > 0) {
                --symbol;
                symbolVector.writeElementAt(symbolIndex, symbol);
                stack.push_back(symbolIndex++);
            } else {
                NativeNaturalType one = stack.pop_back(), zero = stack.pop_back();
                huffmanChildren.writeElementAt(huffmanChildrenIndex*2, zero);
                huffmanChildren.writeElementAt(huffmanChildrenIndex*2+1, one);
                stack.push_back(symbolCount+huffmanChildrenIndex);
                ++huffmanChildrenIndex;
            }
        }
    }

    StaticHuffmanDecoder(Blob& _blob, NativeNaturalType& _offset) :StaticHuffmanCodec(_blob, _offset) {}

    ~StaticHuffmanDecoder() {
        symbolVector.clear();
        huffmanChildren.clear();
    }
};
