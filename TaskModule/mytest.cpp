#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <unordered_map>

#include <initializer_list>
#include <cstdarg>
#include <list>
#include <sstream>
#include <typeinfo>

class Log final
{
    public:
        Log()
        {
        };

        ~Log()
        {
            printf("%s", os.str().c_str());
        }

        template <typename T>
        Log& operator<<(const T &val)
        {
            //static_cast<std::ostringstream &>(*this) << val;
            os << val;
            return *this;
        }

        Log& operator<<(const Log &value)
        {
            //static_cast<std::ostringstream &>(*this) << value.str();
            os << value.str();
            return *this;
        }

        std::string str() const
        {
            return os.str();
        }

    public:
        std::ostringstream os;
};

enum TaskPrio
{
    HI = 0,
    NO = 1,
    LO = 2,
};

class IIOData
{
    public:
        virtual std::string Name() = 0;
        virtual bool GetData(void* pDataBuf, uint32_t nBufSize) = 0;
        virtual bool SetData(void* pData, uint32_t nDataSize) = 0;
};

class ITaskIO
{
    public:
        virtual IIOData* Get(const std::string& strName) = 0;
        virtual void Set(const std::string& strName, IIOData* pPort) = 0;
};

class TaskIO : public ITaskIO
{
    public:
        static TaskIO& GetInstance() {
            static TaskIO io;
            return io;
        }

        IIOData* Get(const std::string& strName) override
        {
            auto it = m_mapIOPort.find(strName);

            if(it != m_mapIOPort.end())
            {
                return it->second;
            }

            return nullptr;
        }

        void Set(const std::string& strName, IIOData* pPort) override
        {
            m_mapIOPort[strName] = pPort;
        }

    private:
        std::map<std::string, IIOData*> m_mapIOPort;
};

class TaskModule
{
    public:
        TaskModule() : m_priority(TaskPrio::NO)
    {
    }

        virtual const std::string Name() const = 0;
        virtual bool IsCondition() const = 0;
        virtual void SetInput(IIOData* p) = 0;
        virtual void SetOutput(IIOData* p) = 0;
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
            //Log() <<"TaskModule: " << Name() << " >> " << task->Name() << "\n";
            m_lstNext.push_back(task);
            return *task;
        }

        TaskModule& Before(const std::initializer_list<TaskModule*>& tasks)
        {
            for (auto& t : tasks)
            {
                //Log() <<"TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
                m_lstNext.push_back(t);
            }

            return *this;
        }

        TaskModule& After(const std::initializer_list<TaskModule*>& tasks)
        {
            for (auto& t : tasks)
            {
                //Log() <<"TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
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

        void SetInput(IIOData* p)
        {
            if(nullptr != p)
            {
                m_Runner.SetInput(p);
            }
        }

        void SetOutput(IIOData* p)
        {
            if(nullptr != p)
            {
                m_mapOutput[p->Name()] = p;
            }
        }

    private:
        TRunner m_Runner;
        std::map<std::string, IIOData*> m_mapOutput;
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

                    //Log() <<(pPrev ? pPrev->Name():"")<< " < " << pCurr->Name() << " > " << (pNext ? pNext->Name():"")<< std::endl;
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
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            //Log() <<"Run class output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class StartZmqSvr
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"StartZmqSvr Run begin\n";

            while(true)
            {
                uint32_t nFlashProgress = 0;
                m_mapInput["nFlashProgress"]->GetData(&nFlashProgress, sizeof(nFlashProgress));
                Log() <<"    StartZmqSvr Run nFlashProgress:" << std::dec << nFlashProgress << "\n";
                if(nFlashProgress >= 100) break;
                usleep(1000 * 100);
            }

            Log() <<"StartZmqSvr Run end\n";

            return 0;
        }


        void SetInput(IIOData* p)
        {
            m_mapInput[p->Name()] = p;
            Log() <<"StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }

    private:
        std::map<std::string, IIOData*> m_mapInput;
};

class ChkActState
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class ChkActState output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class ChkOtaEvt
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class ChkOtaEvt output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class Activate
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class Activate output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class Reboot
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            //Log() <<"Run class output size:" << mapOutput.size() << "\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            //Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class Flash
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Flash Run start\n";

            while(true)
            {
                uint32_t nSocProgress = 0;
                uint32_t nMcuProgress = 0;
                m_mapInput["nSocProgress"]->GetData(&nSocProgress, sizeof(nSocProgress));
                m_mapInput["nMcuProgress"]->GetData(&nMcuProgress, sizeof(nMcuProgress));
                Log() <<"    Flash Run nSocProgress:" << std::dec << nSocProgress << ", nMcuProgress: " << nMcuProgress << "\n";
                IIOData* pOutput = mapOutput["nFlashProgress"];
                pOutput->SetData(&nSocProgress, sizeof(nSocProgress));

                if(nSocProgress >= 100 && nMcuProgress >= 100) break;
                usleep(1000 * 100);
            }

            Log() <<"Flash Run stop\n";

            return 1;
        }


        void SetInput(IIOData* p)
        {
            m_mapInput[p->Name()] = p;
            Log() <<"StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }

    private:
        std::map<std::string, IIOData*> m_mapInput;
};

class FlashSoc
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class FlashSoc begin, output size:" << mapOutput.size() << "\n";
            IIOData* pOutput = mapOutput["nSocProgress"];
            for(uint32_t i = 1; i <= 100; ++i)
            {
                pOutput->SetData(&i, sizeof(i));
                usleep(1000 * 100);
            }
            Log() <<"Run class FlashSoc end\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput:" << std::hex << p << "\n";
        }
};

class FlashMcu
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class FlashMcu begin, output size:" << mapOutput.size() << "\n";
            IIOData* pOutput = mapOutput["nMcuProgress"];
            for(int i = 1; i <= 100; ++i)
            {
                pOutput->SetData(&i, sizeof(i));
                usleep(1000 * 100);
            }
            Log() <<"Run class FlashMcu end\n";
            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

template<typename DataType>
class TIOPort
{
    public:
        virtual bool GetIOPortData(DataType& data)
        {
            return false;
        }

        virtual bool SetIOPortData(const DataType& data)
        {
            return false;
        }
};

template<typename DataType>
class TIOPortAtomic : public TIOPort<DataType>
{
    public:
        bool GetIOPortData(DataType& data) override
        {
            data = m_data.load(std::memory_order_relaxed);
            return true;
        }

        bool SetIOPortData(const DataType& data) override
        {
            m_data.store(data, std::memory_order_relaxed);
            return true;
        }

    protected:
        std::atomic<DataType> m_data;
};

class IOPortString : public TIOPort<std::string>
{
    public:
        bool GetIOPortData(std::string& data) override
        {
            return false;
        }

        bool SetIOPortData(const std::string& data) override
        {
            return false;
        }
};

template<typename ClassIOPort, typename DataType>
class TIOData : public IIOData
{
    public:
        TIOData(const std::string& strName, const DataType& defVal) : m_strName(strName)
        {
            m_IOPort.SetIOPortData(defVal);
        }

        TIOData& InputOf (TaskModule* pMod)
        {
            pMod->SetInput(this);
            return *this;
        }

        TIOData& OutputOf (TaskModule* pMod)
        {
            pMod->SetOutput(this);
            return *this;
        }

        std::string Name() override
        {
            return m_strName;
        }

        bool GetData(void* pDataBuf, uint32_t nBufSize) override
        {
            if(nBufSize == sizeof(DataType))
            {
                DataType data {};
                m_IOPort.GetIOPortData(data);
                memcpy(pDataBuf, &data, sizeof(data));
                return true;
            }

            return false;
        }

        bool SetData(void* pData, uint32_t nDataSize) override
        {
            if(sizeof(DataType) == nDataSize)
            {
                DataType data {};
                memcpy(&data, pData, sizeof(data));
                m_IOPort.SetIOPortData(data);
                return true;
            }

            return false;
        }

    protected:
        std::string m_strName;
        ClassIOPort m_IOPort;
};

void test_module()
{
    TModule<InitGlobal>        modInitGlobal;
    TModule<StartZmqSvr>       modStartZmqSvr;
    TModule<ChkActState>       modChkActState;
    TModule<ChkOtaEvt, true>   modChkOtaEvt;
    TModule<Flash, true>             modFlash;
    TModule<Activate, true>          modActivate;
    //TModule<EndFlash, true>    modEndFlash;
    //TModule<EndAct, true>      modEndAct;
    TModule<Reboot>            modReboot;
    TModule<FlashSoc>          modFlashSoc;
    TModule<FlashMcu>          modFlashMcu;

    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     socProgress("nSocProgress", 0);
    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     mcuProgress("nMcuProgress", 0);
    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     flashProgress("nFlashProgress", 0);

    // io flow
    //socProgress.OutputOf(&modFlashSoc).InputOf(&modStartZmqSvr);
    //mcuProgress.OutputOf(&modFlashMcu).InputOf(&modStartZmqSvr);

    socProgress.OutputOf(&modFlashSoc).InputOf(&modFlash);
    mcuProgress.OutputOf(&modFlashMcu).InputOf(&modFlash);
    flashProgress.OutputOf(&modFlash).InputOf(&modStartZmqSvr);

    // module flow
    modInitGlobal.Before({&modStartZmqSvr, &modChkActState});
    modChkActState >> &modChkOtaEvt.Before({&modFlash, &modActivate});
    //modActivate >> &modEndAct.Before({&modReboot, &modChkOtaEvt});
    modActivate.Before({&modReboot, &modChkOtaEvt});
    //modFlash >> &modEndFlash >> &modChkOtaEvt;
    modFlash >> &modChkOtaEvt;
    modFlash.After({&modFlashSoc, &modFlashMcu});
    //modFlashSoc >> &modFlash;
    //modFlashMcu >> &modFlash;

    modStartZmqSvr.SetPriority(TaskPrio::HI);

    TaskExecutor exec("OTA", { &modInitGlobal });

    exec.Run();
}

template<typename T>
void printValue(T value) {
    Log() <<"传入的值是：" << value << std::endl;
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

    taskflow.dump(Log);
    */
    return 0;
}

