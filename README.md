# data-management-fair

monte carlo simulation of razzle blah blah blah

current multithreading plan (SUBJECT TO CHANGE):

1. create a singular game object to start
2. the forwards and backwards passes will be run in sequence, with the forwards being run multithreaded
3. the forwards pass will be run in parallel, with each thread running a different simulation to speed up the monte carlo simulation
4. the backwards pass will be run in 1 thread, with the game object being updated in a thread-safe manner
5. this will continue until the stop condition is met
