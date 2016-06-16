#include "../Storage/Blob.hpp"

namespace Ontology {

bool unlink(Symbol symbol);

template<bool guarded, typename ElementType>
struct BlobVector {
    Symbol symbol;

    BlobVector() :symbol(0) {}

    ~BlobVector() {
        if(guarded && symbol)
            Ontology::unlink(symbol);
    }

    bool empty() const {
        return (!symbol || Storage::getBlobSize(symbol) == 0);
    }

    NativeNaturalType size() const {
        return (symbol) ? Storage::getBlobSize(symbol)/sizeOfInBits<ElementType>::value : 0;
    }

    ElementType readElementAt(NativeNaturalType at) const {
        assert(symbol && at < size());
        return Storage::readBlobAt<ElementType>(symbol, at);
    }

    void writeElementAt(NativeNaturalType at, ElementType element) const {
        assert(symbol && at < size());
        Storage::writeBlobAt<ElementType>(symbol, at, element);
    }

    ElementType front() const {
        return readElementAt(0);
    }

    ElementType back() const {
        return readElementAt(size()-1);
    }

    void iterate(Closure<void(ElementType)> callback) const {
        for(NativeNaturalType at = 0; at < size(); ++at)
            callback(readElementAt(at));
    }

    void activate() {
        if(!symbol) {
            symbol = Storage::createSymbol();
            assert(guarded && symbol);
        }
    }

    void clear() {
        if(symbol)
            Storage::decreaseBlobSize(symbol, 0, Storage::getBlobSize(symbol));
    }

    void insert(NativeNaturalType at, ElementType element) {
        activate();
        assert(Storage::increaseBlobSize(symbol, at*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value));
        Storage::writeBlobAt<ElementType>(symbol, at, element);
    }

    void erase(NativeNaturalType begin, NativeNaturalType length) {
        assert(symbol);
        assert(Storage::decreaseBlobSize(symbol, begin*sizeOfInBits<ElementType>::value, length*sizeOfInBits<ElementType>::value));
    }

    void erase(NativeNaturalType at) {
        erase(at, 1);
    }

    void push_back(ElementType element) {
        insert(size(), element);
    }

    ElementType pop_back() {
        assert(!empty());
        ElementType element = back();
        erase(size()-1);
        return element;
    }
};

template<typename KeyType, typename ValueType>
struct Pair {
    KeyType key;
    ValueType value;
    Pair() {}
    Pair(KeyType _key) :key(_key) {}
    Pair(KeyType _key, ValueType _value) :key(_key), value(_value) {}
    operator KeyType() {
        return key;
    }
};

template<bool guarded, typename KeyType, typename ValueType = NativeNaturalType[0]>
struct BlobSet : public BlobVector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef BlobVector<guarded, ElementType> Super;

    BlobSet() :Super() {}

    NativeNaturalType find(KeyType key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return key > Super::readElementAt(at);
        });
    }

    bool find(KeyType key, NativeNaturalType& at) const {
        at = find(key);
        return (at < Super::size() && Super::readElementAt(at) == key);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType at;
        if(find(element.key, at))
            return false;
        Super::insert(at, element);
        return true;
    }

    bool eraseElement(ElementType element) {
        NativeNaturalType at;
        if(!find(element.key, at))
            return false;
        Super::erase(at);
        return true;
    }
};

template<bool guarded>
struct BlobIndex : public BlobSet<guarded, Symbol> {
    typedef BlobSet<guarded, Symbol> Super;

    BlobIndex() :Super() {}

    NativeNaturalType find(Symbol key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return Storage::compareBlobs(key, Super::readElementAt(at)) < 0;
        });
    }

    bool find(Symbol element, NativeNaturalType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Storage::compareBlobs(element, Super::readElementAt(at)) == 0);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(find(element, at)) {
            Ontology::unlink(element);
            element = Super::readElementAt(at);
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Symbol element) {
        NativeNaturalType at;
        if(!find(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};

};
