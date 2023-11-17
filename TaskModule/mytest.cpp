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
        TaskModule() : m_priority(TaskPrio::NO), m_bSubAsync(true)
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
            if(task != nullptr)
            {
                m_lstNext.push_back(task);
                task->GetPrevList().push_back(this);
            }

            return *task;
        }

        TaskModule& Before(const std::initializer_list<TaskModule*>& nextTasks)
        {
            std::for_each(nextTasks.begin(), nextTasks.end(), [&](auto& t){
                    m_lstNext.push_back(t);
                    t->GetPrevList().push_back(this);
                    Log() << Name() << " Before " << t->Name() << "\n";
            });

            return *this;
        }

        TaskModule& After(const std::initializer_list<TaskModule*>& prevTasks)
        {
            std::for_each(prevTasks.begin(), prevTasks.end(), [&](auto& t){
                    m_lstPrev.push_back(t);
                    t->GetNextList().push_back(this);
                    Log() << Name() << " After " << t->Name() << "\n";
            });

            return *this;
        }

        TaskModule& SubModule(const std::initializer_list<TaskModule*>& tasks, bool bAsync)
        {
            m_bSubAsync = bAsync;
            m_lstSub.insert(m_lstSub.end(), tasks.begin(), tasks.end());

            if(!m_lstSub.empty())
            {
                // module which has submodules can't be condition
                assert(!IsCondition());
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

        std::list<TaskModule*>& GetSubList()
        {
            return m_lstSub;
        }

        bool IsSubModuleAsync()
        {
            return m_bSubAsync;
        }

    protected:
        TaskPrio m_priority;
        bool m_bSubAsync;
        std::list<TaskModule*> m_lstNext;
        std::list<TaskModule*> m_lstPrev;
        std::list<TaskModule*> m_lstSub;
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
            std::set<std::string> setRelation;
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

                        std::stringstream ss;
                        ss << prev->name() << "->" << curr->name();

                        if(setRelation.count(ss.str()) == 0)
                        {
                            (*curr).succeed(*prev);
                            setRelation.insert(ss.str());
                            Log() << "succeed: " << ss.str() << "\n";
                        }
                    }

                    if(pNext != nullptr)
                    {
                        next = SearchTask(pNext);

                        if(nullptr == next)
                        {
                            next = AddTask(pNext);
                        }

                        std::stringstream ss;
                        ss << curr->name() << "->" << next->name();

                        if(setRelation.count(ss.str()) == 0)
                        {
                            (*curr).precede(*next);
                            setRelation.insert(ss.str());
                            Log() << "precede: " << ss.str() << "\n";
                        }
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
            std::cout << "\n";
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

        tf::Task AddSubTask(TaskModule* pMod)
        {
            return m_taskflow.emplace([=](tf::Subflow& sf) {

                    std::list<TaskModule*>& lstSub = pMod->GetSubList();

                    if(pMod->IsSubModuleAsync())
                    {
                        // when detach create another subflow to hold tf::task
                        // in order to view tasks group
                        sf.emplace([=](tf::Subflow& ssf) {
                            std::for_each(lstSub.begin(), lstSub.end(), [&](auto& t){
                                    Log() << "add sub async:" << t->Name() << "\n";
                                    //here need visit t's module tree to create tf::Task
                                    ssf.emplace([=](){ t->Run(); }).name(t->Name());
                                    });
                            ssf.join();
                            }).name(pMod->Name());

                        sf.detach();
                    }
                    else
                    {
                        std::for_each(lstSub.begin(), lstSub.end(), [&](auto& t){
                                Log() << "add sub sync:" << t->Name() << "\n";
                                //here need visit t's module tree to create tf::Task
                                sf.emplace([=](){ t->Run(); }).name(t->Name());
                                });
                        sf.join();
                    }

                    pMod->Run();
            });
        }

        tf::Task* AddTask(TaskModule* pMod)
        {
            if(pMod->IsCondition())
            {
                m_mapTasks[pMod] = m_taskflow.emplace([=]() { return pMod->Run(); });
            }
            else
            {
                std::list<TaskModule*>& lstSub = pMod->GetSubList();

                if(!lstSub.empty())
                {
                    m_mapTasks[pMod] = AddSubTask(pMod);
                }
                else
                {
                    m_mapTasks[pMod] = m_taskflow.emplace([=]() { (void)pMod->Run(); });
                }
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

                if(pMod->GetPrevList().empty() && pMod->GetNextList().empty())
                {
                    C(pMod, nullptr, nullptr);
                    return;
                }

                // next list

                for (auto& next : pMod->GetNextList())
                {
                    C(pMod, nullptr, next);
                    Traverse(next, visited, C);
                }

                // prev list

                for (auto& prev : pMod->GetPrevList())
                {
                    C(pMod, prev, nullptr);
                    Traverse(prev, visited, C);
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

class FlashEnd
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Flash RunEnd start\n";
            return 1;
        }

        void SetInput(IIOData* p)
        {
            //Log() <<"SetInput: 0x" << std::hex << p << "\n";
        }
};

class FlashSoc
{
    public:
        uint32_t Run(std::map<std::string, IIOData*>& mapOutput)
        {
            Log() <<"Run class FlashSoc begin, output size:" << mapOutput.size() << "\n";
            IIOData* pOutput = mapOutput["nSocProgress"];
            uint32_t nSocProgress = 0;
            pOutput->GetData(&nSocProgress, sizeof(nSocProgress));

            for(uint32_t i = nSocProgress; i <= 100; ++i)
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
            uint32_t nMcuProgress = 0;
            pOutput->GetData(&nMcuProgress, sizeof(nMcuProgress));

            for(int i = nMcuProgress; i <= 100; ++i)
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
    TModule<Flash>             modFlash;
    TModule<FlashEnd, true>    modFlashEnd;
    TModule<Activate, true>    modActivate;
    TModule<Reboot>            modReboot;
    TModule<FlashSoc>          modFlashSoc;
    TModule<FlashMcu>          modFlashMcu;

    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     socProgress("nSocProgress", 80);
    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     mcuProgress("nMcuProgress", 90);
    TIOData<TIOPortAtomic<uint32_t>, uint32_t>     flashProgress("nFlashProgress", 0);

    // io flow
    socProgress.OutputOf(&modFlashSoc).InputOf(&modFlash);
    mcuProgress.OutputOf(&modFlashMcu).InputOf(&modFlash);
    flashProgress.OutputOf(&modFlash).InputOf(&modStartZmqSvr);

    // module flow
    modInitGlobal.Before({&modStartZmqSvr, &modChkActState});
    modChkActState >> &modChkOtaEvt.Before({&modFlash, &modActivate});
    modActivate.Before({&modReboot, &modChkOtaEvt});
    //modFlash.After({&modFlashSoc, &modFlashMcu});
    modFlash.SubModule({&modFlashSoc, &modFlashMcu}, true);
    modFlash >> &modFlashEnd >> &modChkOtaEvt;

    modStartZmqSvr.SetPriority(TaskPrio::HI);

    TaskExecutor exec("OTA", { &modInitGlobal });
    exec.Run();
}

int main() {
    test_module();
    return 0;
}

