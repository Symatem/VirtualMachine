#include "BpTree.hpp"

typedef ArchitectureType Identifier;

namespace Storage {
    struct Blob {
        ArchitectureType size;
        ArchitectureType data[0];
    };
    BpTree<Identifier, Blob*> blobs;
    Identifier maxIdentifier = 0;

    Identifier createIdentifier() {
        // TODO: Identifier free pool
        return ++maxIdentifier;
    }

    void modifiedBlob(Identifier identifier) {
        // TODO
        // blobIndex.eraseElement(symbol);
    }

    void setBlobSize(Identifier identifier, ArchitectureType size, ArchitectureType preserve = 0) {
        BpTree<Identifier, Blob*>::Iterator<false> iter(blobs);
        bool existing = blobs.at(iter, identifier);
        if(size == 0) {
            if(existing) {
                free(iter.getValue());
                blobs.erase(iter);
            }
            return;
        }
        Blob* newBlob = reinterpret_cast<Blob*>(malloc((size+2*ArchitectureSize-1)/ArchitectureSize*ArchitectureSize));
        if(existing) {
            Blob* oldBlob = iter.getValue();
            if(oldBlob->size > 0) {
                ArchitectureType length = min(oldBlob->size, size, preserve);
                if(length > 0)
                    bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(&newBlob->data),
                                    reinterpret_cast<ArchitectureType*>(&oldBlob->data),
                                    0, 0, length);
                free(oldBlob);
            }
        } else {
            blobs.insert(iter, identifier, newBlob);
            iter.end = blobs.layerCount; // TODO
            assert(blobs.at(iter, identifier));
        }
        newBlob->size = size;
        if(size%ArchitectureSize > 0)
            newBlob->data[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        iter.setValue(newBlob);
    }

    void setBlobSizePreservingData(Identifier identifier, ArchitectureType size) {
        setBlobSize(identifier, size, size);
    }

    ArchitectureType getBlobSize(Identifier identifier) {
        BpTree<Identifier, Blob*>::Iterator<false> iter(blobs);
        return (blobs.at(iter, identifier)) ? iter.getValue()->size : 0;
    }

    ArchitectureType* accessBlobData(Identifier identifier) {
        BpTree<Identifier, Blob*>::Iterator<false> iter(blobs);
        assert(blobs.at(iter, identifier));
        return reinterpret_cast<ArchitectureType*>(&iter.getValue()->data);
    }

    int compareBlobs(Identifier a, Identifier b) {
        if(a == b)
            return 0;
        ArchitectureType sizeA = getBlobSize(a),
                         sizeB = getBlobSize(b);
        if(sizeA < sizeB)
            return -1;
        if(sizeA > sizeB)
            return 1;
        if(sizeA == 0)
            return 0;
        return memcmp(accessBlobData(a), accessBlobData(b), (sizeA+7)/8);
    }

    bool overwriteBlobPartial(Identifier dst, Identifier src, ArchitectureType dstOffset, ArchitectureType srcOffset, ArchitectureType length) {
        ArchitectureType dstSize = getBlobSize(dst),
                         srcSize = getBlobSize(src);
        auto end = dstOffset+length;
        if(end <= dstOffset || end > dstSize)
            return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > srcSize)
            return false;
        bitwiseCopy(accessBlobData(dst), accessBlobData(src), dstOffset, srcOffset, length);
        modifiedBlob(dst);
        return true;
    }

    void cloneBlob(Identifier dst, Identifier src) {
        if(dst == src)
            return;
        ArchitectureType srcSize = getBlobSize(src);
        setBlobSize(dst, srcSize);
        bitwiseCopy(accessBlobData(dst), accessBlobData(src), 0, 0, srcSize);
        modifiedBlob(dst);
    }

    template<typename DataType>
    void overwriteBlob(Identifier dst, DataType src) {
        setBlobSize(dst, sizeof(src)*8);
        *reinterpret_cast<DataType*>(accessBlobData(dst)) = src;
        modifiedBlob(dst);
    }

    bool eraseFromBlob(Identifier identifier, ArchitectureType begin, ArchitectureType end) {
        ArchitectureType size = getBlobSize(identifier);
        if(begin >= end || end > size)
            return false;
        ArchitectureType* data = accessBlobData(identifier);
        auto rest = size-end;
        if(rest > 0)
            bitwiseCopy(data, data, begin, end, rest);
        setBlobSizePreservingData(identifier, rest+begin);
        modifiedBlob(identifier);
        return true;
    }

    bool insertIntoBlob(Identifier dst, ArchitectureType* src, ArchitectureType begin, ArchitectureType length) {
        assert(length > 0);
        ArchitectureType dstSize = getBlobSize(dst);
        auto newBlobSize = dstSize+length, rest = dstSize-begin;
        if(dstSize >= newBlobSize || begin > dstSize)
            return false;
        setBlobSizePreservingData(dst, newBlobSize);
        ArchitectureType* dstData = accessBlobData(dst);
        if(rest > 0)
            bitwiseCopy(dstData, dstData, begin+length, begin, rest);
        bitwiseCopy(dstData, src, begin, 0, length);
        modifiedBlob(dst);
        return true;
    }

    void releaseIdentifier(Identifier identifier) {
        // TODO: Identifier free pool
        setBlobSize(identifier, 0);
    }
};
