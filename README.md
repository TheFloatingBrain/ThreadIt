Depends on CallItLater https://github.com/TheFloatingBrain/CallItLater

# ThreadIt (Old)
Designed as a cross platform drop in easy to use threading library, mainly an abstraction layer over std::thread and pthread, with attention to the specific requirements  of platforms like Google Native Client/UCC.

# ThreadIt (Latest)
Designed as a cross platform drop in easy to use threading library, mainly an abstraction layer over std::thread and pthread, with attention to the specific requirements of platforms like Google Native Client/UCC. The later version of ThreadIt is meant to provide additional facilities to make multi - threaded programming easy and as similar to "single threaded" "traditional" programming as possible: making atomics look and act like pointers (atomics are passed to different scopes, they are acquired by dereferencing and released when the particular instance falls from scope), making "atomic pools" (acquire and release a set of variables at the same time to simulate single - threaded programming), and other facilities.
