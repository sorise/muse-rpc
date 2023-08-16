```cpp
template<typename T>
T& SingletonReference(){
    static T instance; //缺点是对象太大的话，会占用很多静态区
    return instance;
}

template <class T>
T* Singleton() {
    static T *instance = new T();
    return instance;
}//问题在于没办法 回收 内存，调用析构函数。

template <class T>
T* SingletonDelete(){
    delete Singleton<T>();
}


template<typename R, typename ...Params>
void real(std::function<R(Params...)> &&t){
    t();
}

template<typename F>
void bind_lambda_call(F && func){
    real(std::function(func));
}
```