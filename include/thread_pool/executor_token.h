#include "executor.h"
#include <future>

#ifndef MUSE_THREAD_POOL_EXECUTOR_TOKEN_HPP
#define MUSE_THREAD_POOL_EXECUTOR_TOKEN_HPP  1

namespace muse::pool{

    template<typename R>
    class ExecutorToken:public Executor{
    public:
        ExecutorToken(std::function<void()> runner, std::future<R> r)
                :Executor(std::move(runner)),resultFuture(std::move(r)) {
        };

        ExecutorToken(const ExecutorToken& other) = delete;
        virtual ~ExecutorToken() = default;
        ExecutorToken(ExecutorToken&& other) noexcept
        :Executor(std::move(other)), resultFuture(std::move(other.resultFuture)){};

        //是否已经完成
        bool isFinish(){ return this->finishState; };

        //任务是否已经被丢弃
        bool isDiscard(){ return this->discardState; };

        //任务是否有错误
        bool isError(){
            if (!isGetValue){
                isGetValue = true;
                try{
                    value = resultFuture.get();
                }catch(...){
                    this->haveException = true;
                }
            }
            return this->haveException;
        };

        R get(){
            isError();
            return value;
        }

    private:
        std::future<R> resultFuture;
        R value;
        bool isGetValue {false};
    };

    template<> class ExecutorToken<void> :public Executor{
    public:
        ExecutorToken(std::function<void()> runner, std::future<void> r)
                :Executor(std::move(runner)),resultFuture(std::move(r)) {
        };

        ExecutorToken(const ExecutorToken& other) = delete;

        ExecutorToken(ExecutorToken&& other) noexcept
                :Executor(std::move(other)), resultFuture(std::move(other.resultFuture)){};

        //是否已经完成
        bool isFinish(){ return this->finishState; };
        //任务是否已经被丢弃
        bool isDiscard(){ return this->discardState; };
        //任务是否有错误
        bool isError(){
            if (!isGetValue){
                isGetValue = true;
                try{
                    resultFuture.get();
                }catch(...){
                    this->haveException = true;
                }
            }
            return this->haveException;
        };

        void get(){
            isError();
        }
    private:
        std::future<void> resultFuture;
        bool isGetValue {false};
    };


    //将任务封装成一个 token！
    template<typename R, typename F, typename ...Args>
    auto make_executor(F&& f, R* r, Args&&...args) -> std::shared_ptr<ExecutorToken<decltype(((*r).*f)(args...))>>{
        using ReturnType = decltype(((*r).*f)(args...));
        const std::shared_ptr<std::packaged_task<ReturnType()>>  runner =
                std::make_shared<std::packaged_task<ReturnType()>>(
                        std::bind(std::forward<F>(f), std::forward<R*>(r),std::forward<Args>(args)...)
                );

        std::function<void()> package = [runner](){
            (*runner)();
        };
        std::shared_ptr<ExecutorToken<ReturnType>> tokenPtr(
                new ExecutorToken<ReturnType>(package, runner->get_future())
        );
        return tokenPtr;
    }

    //普通任务封装成一个 token！
    template<typename R, typename F, typename ...Args>
    auto make_executor(F&& f, R& r, Args&&...args) -> std::shared_ptr<ExecutorToken<decltype((r.*f)(args...))>>{
        using ReturnType = decltype((r.*f)(args...));
        const std::shared_ptr<std::packaged_task<ReturnType()>>  runner =
                std::make_shared<std::packaged_task<ReturnType()>>(
                        std::bind(std::forward<F>(f), std::ref(r),std::forward<Args>(args)...)
                );

        std::function<void()> package = [runner](){
            (*runner)();
        };
        std::shared_ptr<ExecutorToken<ReturnType>> tokenPtr(
                new ExecutorToken<ReturnType>(package, runner->get_future())
        );
        return tokenPtr;
    }

    //将成员函数指针封装成一个 token！
    template< class F, class ...Args >
    auto make_executor(F&& f, Args&&...args) -> std::shared_ptr<ExecutorToken<decltype(f(args...))>>{
        using ReturnType = decltype(f(args...));

        const std::shared_ptr<std::packaged_task<ReturnType()>>  runner =
                std::make_shared<std::packaged_task<ReturnType()>>(
                        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::function<void()> package = [runner](){
            (*runner)();
        };
        std::shared_ptr<ExecutorToken<ReturnType>> tokenPtr(
            new ExecutorToken<ReturnType>(package, runner->get_future())
        );
        return tokenPtr;
    }

    template<typename R>
    static R get_result_executor(const std::shared_ptr<Executor>& token){
        if (token != nullptr){
            auto result = reinterpret_cast<ExecutorToken<R>*>(token.get());
            return result->get();
        }else{
            throw std::runtime_error("The parameter passed in is nullptr");
        }
    }

    template<> void get_result_executor(const std::shared_ptr<Executor>& token){
        if (token != nullptr){
            auto result = reinterpret_cast<ExecutorToken<void>*>(token.get());
            return result->get();
        }else{
            throw std::runtime_error("The parameter passed in is nullptr");
        }
    }

    template<typename R>
    static bool is_discard_executor(const std::shared_ptr<Executor>& token){
        if (token != nullptr){
            auto result = reinterpret_cast<ExecutorToken<R>*>(token.get());
            return result->isDiscard();
        }else{
            throw std::runtime_error("The parameter passed in is nullptr");
        }
    }

    template<typename R>
    static bool is_finish_executor(const std::shared_ptr<Executor>& token){
        if (token != nullptr){
            auto result = reinterpret_cast<ExecutorToken<R>*>(token.get());
            return result->isFinish();
        }else{
            throw std::runtime_error("The parameter passed in is nullptr");
        }
    }

    template<typename R>
    static bool is_error_executor(const std::shared_ptr<Executor>& token){
        if (token != nullptr){
            auto result = reinterpret_cast<ExecutorToken<R>*>(token.get());
            return result->isError();
        }else{
            throw std::runtime_error("The parameter passed in is nullptr");
        }
    }

}


#endif //MUSE_THREAD_POOL_EXECUTOR_TOKEN_HPP
