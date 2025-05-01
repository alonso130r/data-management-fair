# data-management-fair

monte carlo simulation of razzle blah blah blah

current multithreading plan (SUBJECT TO CHANGE):

1. create a singular game object to start
2. threads in the pool will take a pointer to the game object and run a monte carlo sim, then perform a backprop/update step on the learnable parameters
3. threads will then join back to the main thread and update the game object, hopefully without a data race to update parameters
