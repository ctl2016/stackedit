#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <unordered_map>

#include <initializer_list>
#include <cstdarg>
#include <list>
#include <typeinfo>

enum TaskPrio
{
    HI = 0,
    NO = 1,
    LO = 2,
};

class IOPort;

class IIOPort
{
    public:
        virtual std::string GetName() = 0;
        virtual void* GetData() = 0;
        virtual void SetData(void*) = 0;
};

class TaskModule
{
    public:
        TaskModule() : m_priority(TaskPrio::NO)
    {
    }

        virtual const std::string Name() const = 0;
        virtual bool IsCondition() const = 0;
        virtual void SetInput(IOPort* p) = 0;
        virtual void SetOutput(IOPort* p) = 0;
        virtual uint32_t Run() = 0;

        void SetPriority(TaskPrio prio)
        {
            m_priority = prio;
        }

        TaskPrio GetPriority()
        {
            return m_priority;
        }

        TaskModule& operator >> (TaskModule* task)
        {
            //std::cout << "TaskModule: " << Name() << " >> " << task->Name() << "\n";
            m_lstNext.push_back(task);
            return *task;
        }

        TaskModule& Before(const std::initializer_list<TaskModule*>& tasks)
        {
            for (auto& t : tasks)
            {
                //std::cout << "TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
                m_lstNext.push_back(t);
            }

            return *this;
        }

        TaskModule& After(const std::initializer_list<TaskModule*>& tasks)
        {
            for (auto& t : tasks)
            {
                //std::cout << "TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
                m_lstPrev.push_back(t);
            }

            return *this;
        }

        std::list<TaskModule*>& GetNextList()
        {
            return m_lstNext;
        }

        std::list<TaskModule*>& GetPrevList()
        {
            return m_lstPrev;
        }

    protected:
        TaskPrio m_priority;
        std::list<TaskModule*> m_lstNext;
        std::list<TaskModule*> m_lstPrev;
};

template<typename TRunner, bool bIsCondition = false>
class TModule : public TaskModule
{
    public:
        TModule()
    {
    }

        const std::string Name() const override
        {
            return typeid(TRunner).name();
        }

        bool IsCondition() const override
        {
            return bIsCondition;
        }

        uint32_t Run() override
        {
            std::cout << "Run class: " << Name() << " begin\n";
            return m_Runner.Run(m_lstOutput);
        }

        void SetInput(IOPort* p)
        {
            m_Runner.SetInput((IIOPort*)p);
        }

        void SetOutput(IOPort* p)
        {
            if(nullptr != p)
            {
                m_lstOutput.push_back((IIOPort*)p);
            }
        }

    private:
        TRunner m_Runner;
        std::list<IIOPort*> m_lstOutput;
};

class TaskExecutor
{
    public:
        TaskExecutor(const std::string& strName, const std::initializer_list<TaskModule*>& tasks): m_taskflow(strName)
    {
        for (auto& t : tasks)
        {
            std::set<TaskModule*> visited;
            Traverse(t, visited, [&](TaskModule* pCur, TaskModule* pPrev, TaskModule* pNext) {
                    tf::Task* prev = nullptr;
                    tf::Task* next = nullptr;
                    tf::Task* curr = SearchTask(pCur);

                    if(nullptr == curr)
                    {
                    curr = AddTask(pCur);
                    }

                    if(pPrev != nullptr)
                    {
                    prev = SearchTask(pPrev);

                    if(nullptr == prev)
                    {
                    prev = AddTask(pPrev);
                    }

                    (*curr).succeed(*prev);
                    }

                    if(pNext != nullptr)
                    {
                        next = SearchTask(pNext);

                        if(nullptr == next)
                        {
                            next = AddTask(pNext);
                        }

                        (*curr).precede(*next);
                    }

                    //std::cout << (pPrev ? pPrev->Name():"")<< " < " << pCurr->Name() << " > " << (pNext ? pNext->Name():"")<< std::endl;
            });
        }
    }

        void Run(uint32_t nWorkers = 0)
        {
            if(nWorkers == 0)
            {
                nWorkers = std::thread::hardware_concurrency();
            }

            tf::Executor executor(nWorkers);
            executor.run(m_taskflow).get();
            m_taskflow.dump(std::cout);
        }

    protected:

        tf::Task* SearchTask(TaskModule* pMod)
        {
            auto it = m_mapTasks.find(pMod);
            if(it != m_mapTasks.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        tf::Task* AddTask(TaskModule* pMod)
        {
            if(pMod->IsCondition())
            {
                m_mapTasks[pMod] = m_taskflow.emplace([=]() { return pMod->Run(); });
            }
            else
            {
                m_mapTasks[pMod] = m_taskflow.emplace([=]() { (void)pMod->Run(); });
            }

            tf::Task* curr = &m_mapTasks[pMod];
            curr->name(pMod->Name());

            switch(pMod->GetPriority())
            {
                case TaskPrio::HI:
                    curr->priority(tf::TaskPriority::HIGH);
                    break;
                case TaskPrio::NO:
                    curr->priority(tf::TaskPriority::NORMAL);
                    break;
                case TaskPrio::LO:
                    curr->priority(tf::TaskPriority::LOW);
                    break;
            }

            return curr;
        }

        template <typename Callable>
            void Traverse(TaskModule* pMod, std::set<TaskModule*>& visited, Callable C)
            {
                if (visited.count(pMod) > 0 || pMod == nullptr)
                    return;

                visited.insert(pMod);

                if(pMod->GetPrevList().empty() &&pMod->GetNextList().empty())
                {
                    C(pMod, nullptr, nullptr);
                    return;
                }

                // prev list
                for (auto& prev : pMod->GetPrevList())
                {
                    C(pMod, prev, nullptr);
                    Traverse(prev, visited, C);
                }

                // next list
                for (auto& next : pMod->GetNextList())
                {
                    C(pMod, nullptr, next);
                    Traverse(next, visited, C);
                }
            }

    protected:
        std::unordered_map<TaskModule*, tf::Task> m_mapTasks;
        tf::Taskflow m_taskflow;
};

class InitGlobal
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class StartZmqSvr
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }


        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class ChkActState
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class ChkOtaEvt
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class Activate
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class EndFlash
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 1;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class EndAct
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class Reboot
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class output size:" << lstOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class Flash
{
    public:
        Flash()
    {
    }
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class Flash output size:" << lstOutput.size() << "\n";
            IIOPort* pInput = m_lstInput["nSocProgress"];
            uint32_t n = *((uint32_t*)pInput->GetData());
            std::cout << "Flash Run input:" << n << "\n";
            std::cout << "Run class Flash end\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            m_lstInput[p->GetName()] = p;
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }

    private:
        std::map<std::string, IIOPort*> m_lstInput;
};

class FlashSoc
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class FlashSoc end, output size:" << lstOutput.size();
            IIOPort* pOutput = lstOutput.front();
            uint32_t& n = *((uint32_t*)pOutput->GetData());
            n = 100;
            pOutput->SetData(pOutput->GetData());
            std::cout << "Run class FlashSoc end, output size:" << lstOutput.size();
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput:" << std::hex << p << "\n";
        }
};

class FlashMcu
{
    public:
        uint32_t Run(std::list<IIOPort*>& lstOutput)
        {
            std::cout << "Run class end, output size:" << lstOutput.size();
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class IOPort : IIOPort
{
    public:

        IOPort& InputOf (TaskModule* pMod)
        {
            pMod->SetInput(this);
            m_lstInput.push_back(pMod);
            return *this;
        }

        IOPort& OutputOf (TaskModule* pMod)
        {
            pMod->SetOutput(this);
            return *this;
        }

    protected:

        virtual std::string GetName() override
        {
            std::cout << "GetName empty !\n";
            return "";
        }

        virtual void* GetData() override
        {
            std::cout << "GetData nullptr !\n";
            return nullptr;
        }

        virtual void SetData(void* p) override
        {
            std::cout << "SetData:" << std::hex << p << "\n";
        }

    protected:
        std::list<TaskModule*> m_lstInput;
};

class TaskIO
{
};

template<typename DataType>
class TIOPort : public IOPort
{
    public:
        TIOPort(const char* pszName) : m_strName(pszName)
        {
        }

    protected:

        std::string GetName() override
        {
            return "DataName";
        }

        void* GetData() override
        {
            std::cout << "TIOPort::GetData:" << std::hex << &m_data << "\n";
            return (void*)&m_data;
        }

        void SetData(void* p) override
        {
            m_data = *((DataType*)p);
            std::cout << "TIOPort::SetData:" << std::hex << p << "\n";
        }

    protected:
        DataType m_data;
        std::string m_strName;
};

void test_module()
{
    TModule<InitGlobal>        modInitGlobal;
    TModule<StartZmqSvr>       modStartZmqSvr;
    TModule<ChkActState>       modChkActState;
    TModule<ChkOtaEvt, true>   modChkOtaEvt;
    TModule<Flash>             modFlash;
    TModule<Activate>          modActivate;
    TModule<EndFlash, true>    modEndFlash;
    TModule<EndAct, true>      modEndAct;
    TModule<Reboot>            modReboot;
    TModule<FlashSoc>          modFlashSoc;
    TModule<FlashMcu>          modFlashMcu;

    TIOPort<uint32_t>     socProgress("nSocProgress");
    //TIOPort<"nMcuProgress", uint32_t>     mcuProgress;

    // input output flow
    socProgress.OutputOf(&modFlashSoc).InputOf(&modFlash);
    //mcuProgress.OutputOf(&modFlashMcu).InputOf(&modFlash);

    // module flow
    modInitGlobal.Before({&modStartZmqSvr, &modChkActState});
    modChkActState >> &modChkOtaEvt.Before({&modFlash, &modActivate});
    modActivate >> &modEndAct.Before({&modReboot, &modChkOtaEvt});
    modFlash >> &modEndFlash >> &modChkOtaEvt;
    modFlash.After({&modFlashSoc, &modFlashMcu});

    modStartZmqSvr.SetPriority(TaskPrio::HI);

    TaskExecutor exec("OTA", { &modInitGlobal });

    exec.Run();
}

template<typename T>
void printValue(T value) {
    std::cout << "传入的值是：" << value << std::endl;
}

int main() {
    //printValue(5);
    test_module();
    /*
       tf::Executor executor(5);
       tf::Taskflow taskflow("test");

       printf("threads: %d\n", executor.num_workers());

       for(int i=0; i<5; i++) {
       taskflow.emplace([=](){
       printf("thread:%ld\n", pthread_self());
       std::this_thread::sleep_for(std::chrono::seconds(i));
       });
       }

    // submit the taskflow
    tf::Future fu = executor.run(taskflow);

    // request to cancel the submitted execution above
    //fu.cancel();

    // wait until the cancellation finishes
    fu.get();

    taskflow.dump(std::cout);
    */
    return 0;
}

