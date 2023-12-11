#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <unordered_map>
#include <algorithm>
#include <initializer_list>
#include <cstdarg>
#include <list>
#include <sstream>
#include <typeinfo>
#include <assert.h>
#include <any>
#include <type_traits>

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
        virtual bool GetData(std::any& data) = 0;
        virtual bool SetData(const std::any& data) = 0;
};

class ModuleIO
{
    public:
        template<typename T>
        bool SetOutput(const std::string& strName, T data)
        {
            IIOData* pIO = GetIOData(strName, false);

            if(nullptr != pIO)
            {
                return pIO->SetData(data);
            }

            return false;
        }

        template<typename T>
        bool GetOutput(const std::string& strName, T& data)
        {
            IIOData* pIO = GetIOData(strName, false);
            
            if(nullptr != pIO)
            {
                return pIO->GetData(data);
            }

            return false;
        }

        template<typename T>
        bool GetInput(const std::string& strName, T& data)
        {
            IIOData* pIO = GetIOData(strName, true);

            if(nullptr != pIO)
            {
                return pIO->GetData(data);
            }

            return false;
        }

        void SetIOData(IIOData* pIOData, bool bInput)
        {
            if(bInput)
            {
                m_mapInput[pIOData->Name()] = pIOData;
            }
            else
            {
                m_mapOutput[pIOData->Name()] = pIOData;
            }
        }

    protected:

        IIOData* GetIOData(const std::string& strName, bool bInput)
        {
            if(bInput)
            {
                auto it = m_mapInput.find(strName);

                if(it != m_mapInput.end())
                {
                    return it->second;
                }
            }
            else
            {
                auto it = m_mapOutput.find(strName);

                if(it != m_mapOutput.end())
                {
                    return it->second;
                }
            }

            return nullptr;
        }

    protected:
        std::unordered_map<std::string, IIOData*> m_mapInput;
        std::unordered_map<std::string, IIOData*> m_mapOutput;
};

class TModuleIO
{
    public:
        TModuleIO(ModuleIO* pIO) : m_pIO(pIO)
        {
        }

        template<typename T>
        bool SetOutput(const std::string& strName, const T& data)
        {
            bool bSuccess = m_pIO->SetOutput(strName, data);
            assert(bSuccess == true);
            return bSuccess;
        }

        template<typename T>
        T GetOutput(const std::string& strName)
        {
            std::any data {};
            bool bSuccess = m_pIO->GetOutput(strName, data);
            assert(bSuccess == true);
            return std::any_cast<T>(data);
        }

        template<typename T>
        T GetInput(const std::string& strName)
        {
            std::any data {};
            bool bSuccess = m_pIO->GetInput(strName, data);
            assert(bSuccess == true);
            return std::any_cast<T>(data);
        }

    protected:
        ModuleIO* m_pIO;
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
        TModule() : m_tModuleIO(&m_moduleIO)
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
            return m_Runner.Run(&m_tModuleIO);
        }

        void SetInput(IIOData* p)
        {
            if(nullptr != p)
            {
                m_moduleIO.SetIOData(p, true);
                //m_Runner.SetInput(p);
            }
        }

        void SetOutput(IIOData* p)
        {
            if(nullptr != p)
            {
                m_moduleIO.SetIOData(p, false);
            }
        }

    private:
        TRunner m_Runner;
        ModuleIO m_moduleIO;
        TModuleIO m_tModuleIO;
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
        uint32_t Run(TModuleIO* pIO) {
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
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"StartZmqSvr Run begin\n";

            while(true)
            {
                uint32_t nFlashProgress = pIO->GetInput<uint32_t>("nFlashProgress");

                Log() <<"    StartZmqSvr Run nFlashProgress:" << std::dec << nFlashProgress << "\n";
                if(nFlashProgress >= 100) break;
                usleep(1000 * 100);
            }

            Log() <<"StartZmqSvr Run end\n";

            return 0;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }
};

class ChkActState
{
    public:
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Run class ChkActState\n";
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
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Run class ChkOtaEvt\n";
            uint32_t nOtaEvt = pIO->GetInput<uint32_t>("nOtaEvt");
            return nOtaEvt;
        }

        void SetInput(IIOData* p)
        {
            Log() <<"StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }
};

class Activate
{
    public:
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Run class Activate\n";
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
        uint32_t Run(TModuleIO* pIO)
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
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Flash Run start\n";

            while(true)
            {
                uint32_t nSocProgress = pIO->GetInput<uint32_t>("nSocProgress");
                uint32_t nMcuProgress = pIO->GetInput<uint32_t>("nMcuProgress");
                Log() <<"    Flash Run nSocProgress:" << std::dec << nSocProgress << ", nMcuProgress: " << nMcuProgress << "\n";
                pIO->SetOutput<uint32_t>("nFlashProgress", nSocProgress);

                if(nSocProgress >= 100 && nMcuProgress >= 100) break;
                usleep(1000 * 100);
            }

            Log() <<"Flash Run stop\n";

            return 1;
        }


        void SetInput(IIOData* p)
        {
            Log() <<"StartZmqSvr SetInput: " << p->Name() << ", 0x" << std::hex << p << "\n";
        }
};

class FlashEnd
{
    public:
        uint32_t Run(TModuleIO* pIO)
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
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Run class FlashSoc begin\n";
            uint32_t nSocProgress = pIO->GetOutput<uint32_t>("nSocProgress");

            for(uint32_t i = nSocProgress; i <= 100; ++i)
            {
                pIO->SetOutput<uint32_t>("nSocProgress", i);
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
        uint32_t Run(TModuleIO* pIO)
        {
            Log() <<"Run class FlashMcu begin\n";
            uint32_t nMcuProgress = pIO->GetOutput<uint32_t>("nMcuProgress");

            for(uint32_t i = nMcuProgress; i <= 100; ++i)
            {
                pIO->SetOutput<uint32_t>("nMcuProgress", i);
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

class IIOPort
{
    public:
        virtual bool GetIOPortData(std::any& data) = 0;
        virtual bool SetIOPortData(const std::any& data) = 0;
};

template<typename T>
class TIOPort : public IIOPort
{
    public:
        bool GetIOPortData(std::any& data) override
        {
            if constexpr (!std::is_trivially_copyable_v<T>) {
                m_mutex.lock();
                data = m_data;
                m_mutex.unlock();
            }
            else {
                data = m_data.load(std::memory_order_relaxed);
            }

            return true;
        }

        bool SetIOPortData(const std::any& data) override
        {
            if constexpr (!std::is_trivially_copyable_v<T>) {
                m_mutex.lock();
                m_data = std::any_cast<T>(data);
                m_mutex.unlock();
            }
            else {
                m_data.store(std::any_cast<T>(data), std::memory_order_relaxed);
            }

            return true;
        }

    protected:
        std::conditional_t<std::is_trivially_copyable_v<T>, std::atomic<T>, T> m_data;
        std::conditional_t<!std::is_trivially_copyable_v<T>, std::mutex, bool> m_mutex;
};

template<typename ClassIOPort, typename T>
class TIOData : public IIOData
{
    public:
        TIOData(const std::string& strName, const T& defVal) : m_strName(strName)
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

        bool GetData(std::any& data) override
        {
            m_IOPort.GetIOPortData(data);
            return true;
        }

        bool SetData(const std::any& data) override
        {
            m_IOPort.SetIOPortData(data);
            return true;
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
    TModule<Activate, true>    modActivate;
    TModule<FlashEnd, true>    modFlashEnd;
    TModule<Flash>             modFlash;
    TModule<Reboot>            modReboot;
    TModule<FlashSoc>          modFlashSoc;
    TModule<FlashMcu>          modFlashMcu;

    TIOData<TIOPort<uint32_t>, uint32_t> socProgress("nSocProgress", 80);
    TIOData<TIOPort<uint32_t>, uint32_t> mcuProgress("nMcuProgress", 90);
    TIOData<TIOPort<uint32_t>, uint32_t> flashProgress("nFlashProgress", 0);
    TIOData<TIOPort<uint32_t>, uint32_t> otaEvt("nOtaEvt", 0);

    // io flow
    socProgress.OutputOf(&modFlashSoc).InputOf(&modFlash);
    mcuProgress.OutputOf(&modFlashMcu).InputOf(&modFlash);
    flashProgress.OutputOf(&modFlash).InputOf(&modStartZmqSvr);
    otaEvt.InputOf(&modChkOtaEvt);

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

