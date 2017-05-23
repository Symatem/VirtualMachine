#include <Storage/BitVector.hpp>

struct BitVectorContainer {
    BitVector bitVector;

    BitVectorContainer() {}
    BitVectorContainer(Symbol symbol) :bitVector(symbol) {}

    void increaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        bitVector.increaseSize(offset, length);
    }

    void decreaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        bitVector.decreaseSize(offset, length);
    }

    NativeNaturalType getChildOffset(NativeNaturalType at) {
        assert(at == 0);
        return 0;
    }

    NativeNaturalType getChildLength(NativeNaturalType at) {
        return bitVector.getSize();
    }

    BitVector& getBitVector() {
        return bitVector;
    }
};

template<typename _Super>
struct DataStructure : public _Super {
    typedef _Super Super;
    typedef typename Super::ElementType ElementType;
    BitVectorContainer parent;

    DataStructure() :Super(parent) {}
    DataStructure(Symbol symbol) :Super(parent), parent(symbol) {}
};

template<typename _Super>
struct BitVectorGuard : public _Super {
    typedef _Super Super;

    BitVectorGuard() :Super(createSymbol()) {}

    ~BitVectorGuard() {
        releaseSymbol(Super::getBitVector().symbol);
    }
};

#define usingRemappedMethod(name) \
template<typename... Args> \
auto name(Args... args) { \
    return ::name(*this, args...); \
}

template<typename Container>
void setElementCount(Container& container, NativeNaturalType newElementCount) {
    NativeNaturalType elementCount = container.getElementCount();
    if(newElementCount > elementCount)
        container.insertRange(elementCount, newElementCount-elementCount);
    else if(newElementCount < elementCount)
        container.eraseRange(newElementCount, elementCount-newElementCount);
}

template<typename Container>
void swapElementsAt(Container& container, NativeNaturalType a, NativeNaturalType b) {
    typename Container::ElementType elementA = container.getElementAt(a), elementB = container.getElementAt(b);
    container.setElementAt(a, elementB);
    container.setElementAt(b, elementA);
}

template<typename Container>
void iterateElements(Container& container, Closure<void(typename Container::ElementType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getElementAt(at));
}

template<typename Container>
void iterate(Container& container, Closure<void(NativeNaturalType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(at);
}

template<typename Container>
typename Container::ElementType getFirstElement(Container& container) {
    return container.getElementAt(0);
}

template<typename Container>
typename Container::ElementType getLastElement(Container& container) {
    return container.getElementAt(container.getElementCount()-1);
}

template<typename Container>
void insertAsFirstElement(Container& container, typename Container::ElementType element) {
    container.insertElementAt(0, element);
}

template<typename Container>
void insertAsLastElement(Container& container, typename Container::ElementType element) {
    container.insertElementAt(container.getElementCount(), element);
}

template<typename Container>
typename Container::ElementType getAndEraseElementAt(Container& container, NativeNaturalType at) {
    typename Container::ElementType element = container.getElementAt(at);
    container.eraseElementAt(at);
    return element;
}

template<typename Container>
typename Container::ElementType eraseFirstElement(Container& container) {
    return getAndEraseElementAt(container, 0);
}

template<typename Container>
typename Container::ElementType eraseLastElement(Container& container) {
    return getAndEraseElementAt(container, container.getElementCount()-1);
}

template<typename Container>
bool insertElement(Container& container, typename Container::ElementType element) {
    NativeNaturalType at;
    if(container.findKey(element.first, at))
        return false;
    container.insertElementAt(at, element);
    return true;
}

template<typename Container>
bool eraseElementByKey(Container& container, typename Container::ElementType::FirstType key) {
    NativeNaturalType at;
    if(!container.findKey(key, at))
        return false;
    container.eraseElementAt(at);
    return true;
}