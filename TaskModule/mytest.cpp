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
        virtual std::string Name() = 0;
        virtual bool GetData(void* pDataBuf, uint32_t nBufSize) = 0;
        virtual bool SetData(void* pData, uint32_t nDataSize) = 0;
};

class ITaskIO
{
    public:
        virtual IIOPort* Get(const std::string& strName) = 0;
        virtual void Set(const std::string& strName, IIOPort* pPort) = 0;
};

class TaskIO : public ITaskIO
{
    public:
        static TaskIO& GetInstance() {
            static TaskIO io;
            return io;
        }

        IIOPort* Get(const std::string& strName) override
        {
            auto it = m_mapIOPort.find(strName);

            if(it != m_mapIOPort.end())
            {
                return it->second;
            }

            return nullptr;
        }

        void Set(const std::string& strName, IIOPort* pPort) override
        {
            m_mapIOPort[strName] = pPort;
        }

    private:
        std::map<std::string, IIOPort*> m_mapIOPort;
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
            // return m_Runner.Run(&TaskIO::GetInstance());
            return m_Runner.Run(m_mapOutput);
        }

        void SetInput(IOPort* p)
        {
            m_Runner.SetInput((IIOPort*)p);
        }

        void SetOutput(IOPort* p)
        {
            if(nullptr != p)
            {
                m_mapOutput[((IIOPort*)p)->Name()] = (IIOPort*)p;
            }
        }

        template<typename T>
        TModule<TRunner>& Output(const std::string& strName)
        {
            //TIOPort<T> m;
            return *this;
        }

    private:
        TRunner m_Runner;
        std::map<std::string, IIOPort*> m_mapOutput;
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            std::cout << "StartZmqSvr Run begin\n";

            while(true)
            {
                uint32_t nSocProgress = 0;
                uint32_t nMcuProgress = 0;
                m_mapInput["nSocProgress"]->GetData(&nSocProgress, sizeof(nSocProgress));
                m_mapInput["nMcuProgress"]->GetData(&nMcuProgress, sizeof(nMcuProgress));
                std::cout << "    StartZmqSvr Run nSocProgress:" << std::dec << nSocProgress << ", nMcuProgress: " << nMcuProgress << "\n";
                if(nSocProgress >= 100 && nMcuProgress >= 100) break;
                usleep(1000 * 100);
            }

            std::cout << "StartZmqSvr Run end\n";

            return 0;
        }


        void SetInput(IIOPort* p)
        {
            m_mapInput[p->Name()] = p;
            std::cout << "StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }

    private:
        std::map<std::string, IIOPort*> m_mapInput;
};

class ChkActState
{
    public:
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
            return 1;
        }

        void SetInput(IIOPort* p)
        {
            //std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class EndAct
{
    public:
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            //std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class Reboot
{
    public:
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            //std::cout << "Run class output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOPort* p)
        {
            //std::cout << "SetInput: 0x" << std::hex << p << "\n";
        }
};

class Flash
{
    public:
        Flash()
    {
    }
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            return 0;
        }

        void SetInput(IIOPort* p)
        {
        }
};

class FlashSoc
{
    public:
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            std::cout << "Run class FlashSoc begin, output size:" << mapOutput.size() << "\n";
            IIOPort* pOutput = mapOutput["nSocProgress"];
            for(uint32_t i = 1; i <= 100; ++i)
            {
                pOutput->SetData(&i, sizeof(i));
                usleep(1000 * 100);
            }
            std::cout << "Run class FlashSoc end\n";
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
        uint32_t Run(std::map<std::string, IIOPort*>& mapOutput)
        {
            std::cout << "Run class FlashMcu begin, output size:" << mapOutput.size() << "\n";
            IIOPort* pOutput = mapOutput["nMcuProgress"];
            for(int i = 1; i <= 100; ++i)
            {
                pOutput->SetData(&i, sizeof(i));
                usleep(1000 * 100);
            }
            std::cout << "Run class FlashMcu end\n";
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

        virtual std::string Name() override
        {
            return "";
        }

        virtual bool GetData(void* pDataBuf, uint32_t nBufSize) override
        {
            return false;
        }

        virtual bool SetData(void* pData, uint32_t nDataSize) override
        {
            return false;
        }

    protected:
        std::list<TaskModule*> m_lstInput;
};

template<typename DataType>
class TIOPort : public IOPort
{
    public:
        TIOPort(const char* pszName, const DataType& defVal) : m_strName(pszName)
        {
            m_data = defVal;
        }

    protected:

        std::string Name() override
        {
            return m_strName;
        }

        bool GetData(void* pDataBuf, uint32_t nBufSize) override
        {
            if(nBufSize == sizeof(DataType))
            {
                DataType data = m_data.load();
                memcpy(pDataBuf, &data, sizeof(data));
                return true;
            }

            return false;
        }

        bool SetData(void* pData, uint32_t nDataSize) override
        {
            if(sizeof(DataType) == nDataSize)
            {
                m_data.store(*((DataType*)pData));
                IOPort::SetData(pData, nDataSize);
                return true;
            }

            return false;
        }

    protected:
        std::atomic<DataType> m_data;
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

    TIOPort<uint32_t>     socProgress("nSocProgress", 0);
    TIOPort<uint32_t>     mcuProgress("nMcuProgress", 0);

    // io flow
    socProgress.OutputOf(&modFlashSoc).InputOf(&modStartZmqSvr);
    mcuProgress.OutputOf(&modFlashMcu).InputOf(&modStartZmqSvr);

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

