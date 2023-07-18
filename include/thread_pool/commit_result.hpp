//
// Created by remix on 23-7-8.
//

#ifndef MUSE_THREAD_POOL_COMMIT_RESULT_HPP
#define MUSE_THREAD_POOL_COMMIT_RESULT_HPP 1
namespace muse::pool{
    enum class RefuseReason{
        NoRefuse,                       //没有拒绝
        CannotExecuteAgain,             //请勿重复执行
        ThreadPoolHasStoppedRunning,    //线程池已经停止运行
        TaskQueueFulled,                //任务队列已经满了
        MemoryNotEnough,                //任务队列已经满了
    };

    struct CommitResult{
        bool isSuccess;       //是否添加成功
        RefuseReason reason;  //失败原因
    };
}
#endif //MUSE_THREAD_POOL_COMMIT_RESULT_HPP
