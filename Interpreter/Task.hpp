#include "../Ontology/Context.hpp"

struct Task;
bool executePreDefProcedure(Task& task, Symbol procedure);

struct Task {
    Context* context;
    Symbol task, status, frame, block;

    template <typename T>
    T& accessBlobData(SymbolObject* symbolObject) {
        if(symbolObject->blobSize != sizeof(T)*8)
            throwException("Invalid Blob Size");
        return *reinterpret_cast<T*>(symbolObject->blobData.get());
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
            throwException("Invalid Mode Value");
        QueryMethod method = lookup[mode];
        if(method.function == nullptr)
            throwException("Invalid Mode Value");

        std::function<void()> handleNext = [&]() {
            Triple result;
            for(ArchitectureType i = 0; i < method.size; ++i)
                result.pos[i] = triple.pos[method.pos+i];
            callback(result, method.size);
        };

        if(context->indexMode == Context::MonoIndex) {
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
        } else if(context->indexMode == Context::TriIndex && method.index >= 3) {
            method.index -= 3;
            method.pos = 2;
            method.function = &Context::searchGIV;
        }

        triple = triple.reordered(method.index);
        if(!callback) handleNext = nullptr;
        return (context->*method.function)(method.index, triple, handleNext);
    }

    bool valueSetCountIs(Symbol entity, Symbol attribute, ArchitectureType size) {
        return query(9, {entity, attribute, PreDef_Void}) == size;
    }

    bool tripleExists(Triple triple) {
        return query(0, triple) == 1;
    }

    void link(Triple triple) {
        if(!context->link(triple))
            throwException("Already linked", {
                {PreDef_Entity, triple.pos[0]},
                {PreDef_Attribute, triple.pos[1]},
                {PreDef_Value, triple.pos[2]}
            });
    }

    template<bool skip = false>
    void unlink(std::set<Triple> triples, std::set<Symbol> skipSymbols = {}) {
        if(!context->unlink<skip>(triples, skipSymbols))
            throwException("Already unlinked");
    }

    void unlink(Symbol alpha, Symbol beta) {
        std::set<Triple> triples;
        query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            triples.insert({alpha, beta, result.pos[0]});
        });
        if(!triples.empty())
            unlink(triples);
    }

    void unlink(Triple triple) {
        unlink(std::set<Triple>{triple});
    }

    void destroy(Symbol alpha) {
        std::set<Triple> triples;
        auto topIter = context->topIndex.find(alpha);
        if(topIter == context->topIndex.end())
            throwException("Already destroyed");
        for(ArchitectureType i = EAV; i <= VEA; ++i)
            for(auto& beta : topIter->second->subIndices[i])
                for(auto gamma : beta.second)
                    triples.insert(Triple(alpha, beta.first, gamma).normalized(i));
        unlink<true>(triples, {alpha});
    }

    void scrutinizeExistence(Symbol symbol) {
        std::set<Symbol> symbols;
        symbols.insert(symbol);
        while(!symbols.empty()) {
            symbol = *symbols.begin();
            symbols.erase(symbols.begin());
            if(context->topIndex.find(symbol) == context->topIndex.end() ||
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
        std::set<Triple> triples;
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
            throwException("Nonexistent or Ambiguous", {
                {PreDef_Entity, entity},
                {PreDef_Attribute, attribute}
            });
        return value;
    }

    // TODO: Refactor move everything until here into Context

    Symbol indexBlob(Symbol symbol) {
        SymbolObject* symbolObject = context->getSymbolObject(symbol);
        auto iter = context->blobIndex.find(symbolObject);
        if(iter != context->blobIndex.end()) {
            destroy(symbol);
            return iter->second;
        } else {
            context->blobIndex.insert(std::make_pair(symbolObject, symbol));
            return symbol;
        }
    }

    void setStatus(Symbol _status) {
        status = _status;
        setSolitary({task, PreDef_Status, status});
    }

    template<bool unlinkHolds, bool setBlock>
    void setFrame(Symbol _frame) {
        assert(frame != _frame);
        if(_frame == PreDef_Void) {
            block = PreDef_Void;
        } else {
            link({task, PreDef_Holds, _frame});
            setSolitary({task, PreDef_Frame, _frame});
            if(setBlock)
                block = getGuaranteed(_frame, PreDef_Block);
        }
        if(unlinkHolds)
            unlink({task, PreDef_Holds, frame});
        if(frame != PreDef_Void)
            scrutinizeExistence(frame);
        frame = _frame;
    }

    void throwException(const char* message, std::set<std::pair<Symbol, Symbol>> links = {}) {
        assert(task != PreDef_Void && frame != PreDef_Void);
        links.insert(std::make_pair(PreDef_Message, context->createFromData(message)));
        Symbol parentFrame = frame;
        block = context->create(links);
        setFrame<true, false>(context->create({
            {PreDef_Holds, parentFrame},
            {PreDef_Parent, parentFrame},
            {PreDef_Holds, block},
            {PreDef_Block, block}
        }));
        link({frame, PreDef_Procedure, PreDef_Exception}); // TODO: debugging
        throw Exception{};
    }

    bool popCallStack() {
        assert(task != PreDef_Void);
        if(frame == PreDef_Void)
            return false;
        assert(context->topIndex.find(frame) != context->topIndex.end());
        Symbol parentFrame;
        bool parentExists = getUncertain(frame, PreDef_Parent, parentFrame);
        if(!parentExists) {
            parentFrame = PreDef_Void;
            setStatus(PreDef_Done);
        }
        setFrame<true, true>(parentFrame);
        return parentExists;
    }

    Symbol popCallStackTargetSymbol() {
        Symbol TargetSymbol;
        bool target = getUncertain(block, PreDef_Target, TargetSymbol);
        assert(popCallStack());
        if(target)
            return TargetSymbol;
        return block;
    }

    void clear() {
        if(task == PreDef_Void)
            return;
        while(popCallStack());
        destroy(task);
        task = status = frame = block = PreDef_Void;
    }

    bool step() {
        if(!running())
            return false;

        Symbol parentBlock = block, parentFrame = frame, execute,
               procedure, next, catcher, staticParams, dynamicParams;
        if(!getUncertain(parentFrame, PreDef_Execute, execute)) {
            popCallStack();
            return true;
        }

        try {
            block = context->create();
            procedure = getGuaranteed(execute, PreDef_Procedure);
            setFrame<true, false>(context->create({
                {PreDef_Holds, parentFrame},
                {PreDef_Parent, parentFrame},
                {PreDef_Holds, block},
                {PreDef_Block, block},
                {PreDef_Procedure, procedure} // TODO: debugging
            }));

            if(getUncertain(execute, PreDef_Static, staticParams))
                query(12, {staticParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    link({block, result.pos[0], result.pos[1]});
                });

            if(getUncertain(execute, PreDef_Dynamic, dynamicParams))
                query(12, {dynamicParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                        link({block, result.pos[0], resultB.pos[0]});
                    });
                });

            if(getUncertain(execute, PreDef_Next, next))
                setSolitary({parentFrame, PreDef_Execute, next});
            else
                unlink(parentFrame, PreDef_Execute);

            if(getUncertain(execute, PreDef_Catch, catcher))
                link({frame, PreDef_Catch, catcher});

            if(!executePreDefProcedure(*this, procedure))
                link({frame, PreDef_Execute, getGuaranteed(procedure, PreDef_Execute)});
        } catch(Exception) {
            executePreDefProcedure(*this, PreDef_Exception);
        }

        return true;
    }

    bool uncaughtException() {
        assert(task != PreDef_Void);
        return tripleExists({task, PreDef_Status, PreDef_Exception});
    }

    bool running() {
        assert(task != PreDef_Void);
        return tripleExists({task, PreDef_Status, PreDef_Run});
    }

    void executeFinite(ArchitectureType n) {
        if(task == PreDef_Void)
            return;
        setStatus(PreDef_Run);
        for(ArchitectureType i = 0; i < n && step(); ++i);
    }

    void executeInfinite() {
        if(task == PreDef_Void)
            return;
        setStatus(PreDef_Run);
        while(step());
    }

    void deserializationTask(Symbol input, Symbol package = PreDef_Void) {
        clear();

        block = context->create({
            {PreDef_Holds, input}
        });
        if(package == PreDef_Void)
            package = block;
        Symbol staticParams = context->create({
            {PreDef_Package, package},
            {PreDef_Input, input},
            {PreDef_Target, block},
            {PreDef_Output, PreDef_Output}
        }), execute = context->create({
            {PreDef_Procedure, PreDef_Deserialize},
            {PreDef_Static, staticParams}
        });
        task = context->create();
        setFrame<false, false>(context->create({
            {PreDef_Holds, staticParams},
            {PreDef_Holds, execute},
            {PreDef_Holds, block},
            {PreDef_Block, block},
            {PreDef_Execute, execute}
        }));
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = PreDef_Void;
        if(query(9, {block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Symbol next = context->create({
                {PreDef_Procedure, result.pos[0]}
            });
            if(prev == PreDef_Void)
                setSolitary({frame, PreDef_Execute, next});
            else
                link({prev, PreDef_Next, next});
            prev = next;
        }) == 0)
            return false;

        executeInfinite();
        return true;
    }
};