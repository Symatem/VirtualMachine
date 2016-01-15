#include "Blob.hpp"

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

typedef ArchitectureType ReferenceType;
const ArchitectureType
      bitsPerPage = 1<<15,
      mmapBucketSize = 1<<24,
      mmapMaxPageCount = 2000*512;

class BasePage {
    public:
    ArchitectureType transaction;
};

class Storage {
    int file;
    uint8_t* ptr;
    public:
    ReferenceType maxReference;

    ArchitectureType bytesForPages(ArchitectureType pageCount) {
        return (pageCount*bitsPerPage+mmapBucketSize-1)/mmapBucketSize*mmapBucketSize/8;
    }

    Storage() {
        file = open("storage", O_RDWR|O_CREAT);
        if(file < 0) {
            perror("open failed");
            exit(1);
        }
        maxReference = lseek(file, 0, SEEK_END)/(bitsPerPage/8);
        if(maxReference == 0) maxReference = 1;
        ptr = reinterpret_cast<uint8_t*>(MMAP_FUNC(NULL, bytesForPages(mmapMaxPageCount), PROT_NONE, MAP_FILE|MAP_SHARED, file, 0));
        if(ptr == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }
        mapPages(maxReference);
    }

    ~Storage() {
        if(maxReference)
            munmap(ptr, bytesForPages(maxReference));
        if(ftruncate(file, bytesForPages(maxReference)) != 0) {
            perror("ftruncate failed");
            exit(1);
        }
        if(close(file) < 0) {
            perror("close failed");
            exit(1);
        }
    }

    void mapPages(ArchitectureType pageCount) {
        if(maxReference)
            munmap(ptr, bytesForPages(maxReference));
        if(pageCount >= mmapMaxPageCount) {
            perror("memory exhausted");
            exit(1);
        }
        maxReference = pageCount;
        if(ftruncate(file, bytesForPages(maxReference)) != 0) {
            perror("ftruncate failed");
            exit(1);
        }
        if(MMAP_FUNC(ptr, bytesForPages(maxReference),
                     PROT_READ|PROT_WRITE, MAP_FIXED|MAP_FILE|MAP_SHARED,
                     file, 0) != ptr) {
            perror("re mmap failed");
            exit(1);
        }
    }

    template<typename PageType>
    PageType* dereferencePage(ReferenceType reference) {
        assert(reference < maxReference);
        return reinterpret_cast<PageType*>(ptr+bitsPerPage/8*reference);
    }

    ReferenceType aquirePage();

    void releasePage(ReferenceType reference);
};

class PagePool {
    typedef uint16_t IndexType;
    ReferenceType rootReference;

    public:
    class Page : public BasePage {
        public:
        static const ArchitectureType
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8+sizeof(ReferenceType)*8,
            BodyBits = bitsPerPage-HeaderBits,
            ReferenceBits = sizeof(ReferenceType)*8,
            Capacity = (BodyBits-ReferenceBits)/ReferenceBits,
            ReferencesBitOffset = architecturePadding(HeaderBits);

        IndexType count;
        ReferenceType chain, references[Capacity];

        void init(ReferenceType _chain = 0) {
            count = 0;
            chain = _chain;
        }

        bool contains(ReferenceType item) {
            for(ArchitectureType i = 0; i < count; ++i)
                if(references[i] == item)
                    return true;
            return false;
        }

        void debugPrint(std::ostream& stream) {
            stream << count << std::endl;
            for(ArchitectureType i = 0; i < count; ++i) {
                if(i > 0)
                    std::cout << ", ";
                std::cout << references[i];
            }
            std::cout << std::endl;
        }

        void push(ReferenceType reference) {
            assert(count < Capacity);
            references[count++] = reference;
        }

        ReferenceType pop() {
            assert(count > 0);
            return references[--count];
        }
    };

    void init() {
        rootReference = 0;
    }

    bool isEmpty() const {
        return (rootReference == 0);
    }

    bool contains(Storage* storage, ReferenceType item) {
        ReferenceType reference = rootReference;
        while(reference != 0) {
            auto page = storage->template dereferencePage<Page>(reference);
            if(reference == item || page->contains(item))
                return true;
            reference = page->chain;
        }
        return false;
    }

    void debugPrint(Storage* storage, std::ostream& stream) {
        ReferenceType reference = rootReference;
        while(reference != 0) {
            auto page = storage->template dereferencePage<Page>(reference);
            stream << "Page " << reference << " ";
            page->debugPrint(stream);
            reference = page->chain;
        }
    }

    void push(Storage* storage, ReferenceType reference) {
        if(rootReference == 0) {
            rootReference = reference;
            auto page = storage->template dereferencePage<Page>(rootReference);
            page->init();
            return;
        }
        auto page = storage->template dereferencePage<Page>(rootReference);
        if(page->count == Page::Capacity) {
            ReferenceType chain = rootReference;
            rootReference = reference;
            page = storage->template dereferencePage<Page>(rootReference);
            page->init(chain);
        } else
            page->push(reference);
    }

    ReferenceType pop(Storage* storage) {
        assert(!isEmpty());
        auto page = storage->template dereferencePage<Page>(rootReference);
        if(page->count == 0) {
            ReferenceType reference = rootReference;
            rootReference = page->chain;
            return reference;
        } else
            return page->pop();
    }
};

class RootPage : public BasePage {
    public:
    PagePool freePool;
};

ReferenceType Storage::aquirePage() {
    ReferenceType reference;
    auto rootPage = dereferencePage<RootPage>(0);
    if(rootPage->freePool.isEmpty()) {
        mapPages(maxReference+1);
        reference = maxReference-1;
    } else
        reference = rootPage->freePool.pop(this);
    return reference;
}

void Storage::releasePage(ReferenceType reference) {
    if(reference == maxReference-1)
        mapPages(maxReference-1);
    else
        dereferencePage<RootPage>(0)->freePool.push(this, reference);
}