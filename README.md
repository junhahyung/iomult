# I/O multiplexing Server

### High-performance web server that can process about 100,000 requests per second. Use I/O multiplexing and thread pool to handle concurrent requests from clients

### Uses Tcmalloc

### How to Compile

### Run
- ./server [random server name] [addr to file] [port]
- ./client1 ip port numthread numreq queryword
- ./client2 ip port // >search word

## Code Explanation
### server.c
IO multiplexing 서버이다. Select가 멀티플렉싱을 해주면 들어온것이 listen fd 인지, connect fd 인지 구분한다. Listen fd면 새로운 connection을 만들어준다. Connect fd이면 이미 연결된 connection에 데이터가 들어온 것이므로 argument를 만들고, 이를 쓰레드에 전달해주어 쓰레드에 일을 시킨다. 이때 일을 시키는 방식을 Thread pool로 구현했다. Thread pool은 먼저 thpool_init(num) 함수로 working thread를 만들어놓고 기다린다. 이때 num은 만들고자하는 쓰레드의 개수고, HW2와 같이 pthread_create함수를 이용해 구현했다. 각각의 thread가 하는 일은 thead_do에 구현했는데, while(keepalive) 형식으로 thread 가 파괴되기 전까지 무한루프를 돌도록 했다. 루프에 들어가면, 일이 생기기 전까지(job queue를 확인해서) pthread_cond_wait이라는 api로 sleep을 하고 기다리고 있다가, 일이 생기면 깨어난다. 깨어나서 일을 할당받고, 일을 처리하도록 하였다.
Inverted index를 통해 결과를 얻으면, tcmalloc을 알맞게 해준 후 protocol 형식에 맞는 packet을 만들고 보냈다. Write의 경우 4096B까지 한번에 보내지는 것이 guarantee 되지만,
   
 read의 경우에는 그렇지 않아 심혈을 기울였다. 어떤 방식으로 read를 했냐하면, 먼저 모든 패킷의 크기는 최소 8B이기 때문에(total length, msg type) 8B 이상을 읽을때까지 계속해서 read를 실행시킨다. 이제 처음 8B를 읽었다는 것이 guarantee되면, total length에 접속해 전체 길이를 파악한다. 그리고 다시 while loop을 돌며 해당 길이만큼 읽을때까지 계속해서 read를 하게 해준다.
 
### Client1.c
Thread 를 numthread 만큼 만들고, 각 thread 안에서 numrequest 만큼 forloop을 통해 새로운 connection을 만들고 데이터를 보냈다. 즉, numthread * numrequest 만큼의 커넥션과 데이터를 보냈다.
### Client2.c
Client1과 동일하지만 numthread 와 numrequest를 1로 각각 하였고, 받은 결과를 print out하도록 하였다.

### tcmalloc.c
TCmalloc을 사용하기 위해서는 threadcache init을 파일별로, 쓰레드 별로 해주어야 한다. 이때 문제가 생겼던 점이 tcmalloc과 malloc을 같이 사용하면 문제가 될 수 있다는 사실을 알게 되었다. 메모리상으로 tcmalloc에서 sbrk를 해서 얻은 공간을 malloc이 사용하는 사실을 확인하였다. 따라서 malloc을 모두 tcmalloc으로 바꿔 tcmalloc으로 통일해서 구현했고, 다행히 문제없이 잘 동작하였다.
