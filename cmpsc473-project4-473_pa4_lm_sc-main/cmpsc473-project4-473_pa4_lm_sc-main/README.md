Leran Ma , lkm5463@psu.edu
Sishi Cheng , smc6823@psu.edu




@Test1: 1, 1000, 5890000
@Test2: 2, 1000, 7630000
@Test3: 4, 1000, 14900000
@Test4: 8, 1000, 15340000
@Test5: 16, 1000, 15410000
@Test6: 32, 1000, 15730000
@Test7: 8, 100, 15350000
@Test8: 8, 1000, 15340000
@Test9: 8, 10000, 15340000




Thoughts on the variation of time:
For this problem, increasing the number of mapper threads does not help reduce the running time. This is probably because most of the code in the buffer_send function and buffer_receive function deals with the shared data, the buffer. Thus, we have to use mutex lock to prevent data race and race condition. As a result, all mappers and the one reducer wait and take turns to try accessing the buffer. Only one thread is allowed to access the buffer at any given time. The context switching overhead grows with the number of mapper threads. Moreover, there's only one reducer thread. Increasing the number of mapper threads does not increase the consume rate. Therefore, the running time increases with the number of mapper threads.

Besides, increasing the buffer size almost does not affect the running time. This is probably because when there is only one reducer thread, adding more redundant space to the buffer can not help improve the consume rate. Hence, the running time almost remains constant when the buffer size increases.
