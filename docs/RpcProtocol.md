## RPC 调用协议

---

**请求调用参数头**，调用体存放参数
```javascript
{
    name: "functionName" //方法名称
}
```

返回调用,返回头如下，返回体存储返回值。
```
{
    ok: true/false 是否调用成功
    failure: {
        0 : 成功
        1 : 参数错误,
        2 : 指定方法不存在
        3 : 客户端内部异常，请求还没有到服务器 
        4 : 服务器内部异常，请求到服务器了，但是处理过程有异常       
    } 
}
```