// CPU Schduling Simulation
// 연산(Computation)과 입출력(I/O) 요청을 번갈아 수행하는
// process들에 대해 CPU scheduling을 수행하는 프로그램

// Note:
// 1) cburst 와 ioburst는 프로그램 실행 전에 한 번 랜덤하게 생성되면 해당 프로그램이 종료될 때까지 바뀌지 않는다.
//   즉 매번 같은 상태 전이가 이루어져도 cburst와 ioburst의 값은 항상 일정하다고 가정.
// 2) 본 시뮬레이션에서 SJF는 비선점형 방식 (선점형 SJF는 별도로 STCF, PSJF 등으로 불리기 때문에 제시되지 않은 관계로 비선점형으로 구현)
// 3) test case로 주어진 예는 프로세스의 숫자도 적고 CPU time 등의 차이가 크지 않아서 스케쥴링 알고리즘 별 성능의 차이가
//   크게 나타나지 않았다. 따라서 프로세스의 수를 15개로 늘리고 CPU time의 편차를 크게 둔 test case를 새로 만들어서 비교해 보았다.
// 4) Round Robin 에서 quantum의 값이 클 수록 FCFS와 결과가 같아지고, quantum의 값이 충분히 크다면 FCFS와 결과가 같다.
//   Rount Robin은 스케쥴러의 성능 척도 중에서 response time을 최소화 하는데 유용한데 본 과제에서는 response time을 따로 구해서
//   비교하는 것은 생략되어 있는 관계로 turn around time, waiting time 기준으로는 가장 성능이 떨어지게 나온다.
//   또한 quantum값이 작을 수록 context switching overhead가 더 많이 발생하게 되는데 이 부분도 생략되었다.
// 5) FCFS와 SJF의 비교에서는 평균적으로 SJF의 성능이 더 좋은 것으로 나왔다.

#include <stdbool.h>  // #define TRUE 1; #define FALSE 0; 대신 C99부터 추가된 bool 자료형을 이용하기 위해
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strcmp() 함수를 사용하기 위해
#include <time.h>    // 난수 생성을 위해 srand()를 time(NULL)로 초기화 해주기 위해

// pcb: process control block 자료형
typedef struct pcb {
    int pid;  // process id
    int A;    // 프로세스가 도착한 시각
    int C;    // 프로세스가 종료될 때까지 필요로 하는 총 CPU time
    int B;    // cburst는 0과 B사이의 랜덤한 정수
    int IO;   // ioburst는 0과 IO사이의 랜덤한 정수
    struct pcb* link;

    int remaining_cpu_time;
    int cburst;  // CPU burst time
    int remaining_cburst;
    int ioburst;  // IO burst time
    int remaining_ioburst;
    int remaining_quantum;

    // summury data 출력을 위한 변수들
    int turnaround_time;     // completion_time - A(=arrival time)
                             // turnaround time은 CPU time, IO time, Waiting time의 합으로 검산이 가능
    int ready_start_time;    // ready_queue에 들어간 시각
    int ready_end_time;      // ready_queue에서 나온 시각
    int waiting_time;        // ready_end_time - ready_start_time
    int blocked_start_time;  // block_queue에 들어간 시각
    int blocked_end_time;    // block_queue에서 나온 시각
    int blocked_time;        // blocked_end_time - blocked_start_time
    int completion_time;     // 해당 프로세스가 종료된 시각

} pcb;

// Linked List 로 Queue를 구현해주기 위해 node 자료형을 선언
typedef struct node {
    pcb process;
    struct node* next;
} node;

// 문제에서 주어진 형태의 queue_t 자료형
typedef struct queue_t {
    pcb* first;  // head, front 등으로도 불림
    pcb* last;   // tail, rear  등으로도 불림
    int count;   // queue 안에 있는 data의 수
} queue_t;

// global variables
int number_of_processes = 0;       // 총 프로세스의 수
bool is_all_finished = false;      // 모든 프로세스가 종료되면 true
char* sched_algorithm_title = "";  // 선택한 스케쥴러 알고리즘을 저장하기 위한 char* 타입
int quantum = 0;                   // Round Robin의 preemptive 부분 구현을 위한 time slice

int finishing_time = 0;  // 모든 프로세스를 마치고 프로그램이 끝난 시각, 즉 마지막 프로세스가 종료된 시각
int idle_time = 0;       // CPU 유휴시간 (running queue가 비어있으면 ++)
double cpu_util = 0;     // CPU Utilization 0~1 사이의 값
double io_util = 0;      // IO Utilization 0~1 사이의 값
int total_turnaround_time = 0;
int total_waiting_time = 0;
int total_blocked_time = 0;
double avg_turnaround_time = 0;  // Average Turnaround Time
double avg_waiting_time = 0;     // Average Waiting Time
double throughput_per_unit = 0;  // 단위 시간 당 throuughput

void queue_init(queue_t* queue);            // queue를 초기화
bool is_empty(queue_t* queue);              // queue가 비어있으면 true를 return
void enqueue(queue_t* queue, pcb process);  // queue의 last(맨 뒤)에  process를 넣음
pcb dequeue(queue_t* queue);                // queue의 first(맨 앞)에서 process를 꺼냄 (꺼낸 pcb를 return)

// 메인 시작 ./sched [filename] [scheduling method: fcfs, sjf, or rr]
int main(int argc, char** argv) {
    srand(time(NULL));  // 난수 생성

    if (argc != 3 && argc != 4) {
        printf("Usage: ./sched [filename] [scheduling algorithm: fcfs, sjf, or rr] [quantum]\nIf you use rr, then you must provide quantum argument.\n");
        return 1;
    }
    FILE* file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Error! Could not open the file\n");
        return 1;
    }

    fscanf(file, "%d", &number_of_processes);  // 프로세스의 수를 저장
    printf("\nnumber of processes: %d\n\n", number_of_processes);

    pcb* process;
    // 프로세스 구조체 배열을 동적 할당할 포인터 변수 선언
    /* number_of_processes 만큼의 크기의 프로세스 구조체의 배열을 동적으로 할당 */

    process = (pcb*)malloc(number_of_processes * sizeof(pcb));
    for (int i = 0; i < number_of_processes; ++i) {
        // txt파일로부터 A, C, B, IO를 각각 불러옴
        fscanf(file, "%d %d %d %d", &process[i].A, &process[i].C, &process[i].B, &process[i].IO);

        // rand() % x 에서 x가 0이면 floating point exception이 발생하므로
        process[i].cburst = (rand() % (process[i].B + 1));
        process[i].ioburst = (rand() % (process[i].IO + 1));

        // cburst 가 0이 되어서는 안되므로 랜덤으로 0이 나왔으면 1을 배정함
        if (process[i].cburst == 0) {
            process[i].cburst = 1;
        }

        // 계산을 위해 다음 (remaining) 변수들을 초기화함
        process[i].remaining_cburst = process[i].cburst;
        process[i].remaining_ioburst = process[i].ioburst;
        process[i].remaining_cpu_time = process[i].C;
        process[i].waiting_time = 0;
        process[i].blocked_time = 0;
    }
    // 열어준 파일포인터 file을  닫아준다.
    fclose(file);

    queue_t ready_queue;    // ready state: 각 알고리즘에 따라 ready queue에 들어가는 순서가 다름
    queue_t running_queue;  // running state: 프로세스가 CPU작업을 수행
    queue_t blocked_queue;  // blocked state: 프로세스가 IO작업을 수행

    queue_t complete_queue;  // 각 프로세스가 종료되면 프로세스의 parameter들을 출력하기 위한 임시 queue

    // 각 queue들을 초기화
    queue_init(&ready_queue);
    queue_init(&running_queue);
    queue_init(&blocked_queue);
    queue_init(&complete_queue);

    int count_time = 0;      //  while (!is_all_finished){...} 반복문을 수행하면서 1씩 증가 (time unit을 구현)
    int process_index = 0;   // 프로세스를 ready큐로 넣어줄 때 for문을 위한 index 변수
    int finished_count = 0;  // 완료된 프로세스의 수. number_of_processes와 같아지면 while문 종료

    //
    //  First Come First Served (FCFS)
    //
    // strcmp는 argv[2]가 "fcfs"랑 일치할 때 0을 return하므로
    if (strcmp(argv[2], "fcfs") == 0) {
        sched_algorithm_title = "First Come First Served (FCFS)";

        // 도착 시간 순서대로 버블정렬
        for (int i = 0; i < number_of_processes; ++i) {
            for (int j = i + 1; j < number_of_processes; ++j) {
                if (process[i].A > process[j].A) {
                    pcb tmp = process[i];
                    process[i] = process[j];
                    process[j] = tmp;
                }
            }
            // 정렬된 순서대로 process id를 부여함
            process[i].pid = i;  // pid
        }

        // while loop 시작 count time++
        while (!is_all_finished) {
            // process의 A(도착시각)를 체크해서 arrival time이 되면 ready queue에 enqueue
            for (int i = process_index; i < number_of_processes; ++i) {
                if (process[i].A == count_time) {
                    process[i].ready_start_time = count_time;
                    enqueue(&ready_queue, process[i]);
                    process_index++;
                }
            }

            // printf("time: %d ready_queue count: %d\n", count_time, ready_queue.count);

            // CPU가 1개로 가정했기 때문에 running state에는 1개의 프로세스만 온다
            //running_queue가 비어 있는 상태면 ready queue에서 프로세스를 가져옴
            if (is_empty(&running_queue)) {
                // printf("time: %d running_queue is empty\n", count_time);

                //그런데 ready_queue도 비어있는 경우에는 cpu를 활용할 수 없으므로 idle_time++
                if (is_empty(&ready_queue)) {
                    idle_time++;

                }
                // ready_queue에 process가 있는 경우 가져와서 running으로
                else {
                    pcb tmp = dequeue(&ready_queue);
                    tmp.ready_end_time = count_time;
                    tmp.waiting_time += (tmp.ready_end_time - tmp.ready_start_time);
                    enqueue(&running_queue, tmp);
                }
            }
            // running_queue에 프로세스가 있으면
            else {
                running_queue.first->remaining_cburst--;
                running_queue.first->remaining_cpu_time--;

                // CPU time만큼 running state에 있었으면 프로세스를 종료하고 complete_queue로 보낸다
                if (running_queue.first->remaining_cpu_time == 0) {
                    pcb tmp = dequeue(&running_queue);
                    tmp.completion_time = count_time;
                    tmp.turnaround_time = tmp.completion_time - tmp.A;

                    total_turnaround_time += tmp.turnaround_time;
                    total_waiting_time += tmp.waiting_time;
                    total_blocked_time += tmp.blocked_time;

                    enqueue(&complete_queue, tmp);
                    finished_count++;
                }
                // 현재 running process 의 cpu burst time이 0가 되면
                else if (running_queue.first->remaining_cburst == 0) {
                    // IO작업이 없는 프로세스일 경우 blocked process로 보내지 않고
                    // 바로 이어서 다음 작업을 시작함
                    if (running_queue.first->ioburst == 0) {
                        running_queue.first->remaining_cburst = running_queue.first->cburst;
                    }
                    // ioburst != 0 일 경우 IO작업을 하기 위해 blocked queue로 보냄
                    else {
                        pcb tmp = dequeue(&running_queue);
                        // IO작업을 위해 blocked_queue로 보낸다
                        tmp.blocked_start_time = count_time;
                        // remaining_ioburst ioburst 를 다시 초기화 해준다
                        tmp.remaining_ioburst = tmp.ioburst;
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // blocked_queue가 비어있지 않으면
            // io도 한 번에 1개의 프로세스만 io작업을 한다고 가정
            if (!is_empty(&blocked_queue)) {
                // blocked_queue에 있는 프로세스의 수만큼 for문을 돌리기 위해
                int process_count = blocked_queue.count;
                for (int i = 0; i < process_count; ++i) {
                    pcb tmp = dequeue(&blocked_queue);
                    tmp.remaining_ioburst--;

                    //block queue에 있는 io작업이 끝난 프로세스가 있으면
                    if (tmp.remaining_ioburst == 0) {
                        tmp.blocked_end_time = count_time;
                        tmp.blocked_time += (tmp.blocked_end_time - tmp.blocked_start_time);
                        tmp.remaining_cburst = tmp.cburst;
                        tmp.ready_start_time = count_time;
                        enqueue(&ready_queue, tmp);
                    }
                    // 아직 IO 작업이 남아있으면 다시 blocked_queue에 넣어준다
                    else {
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // 모든 프로세스가 완료되면 Finishing time을 기록하고 while반복문을 빠져나간다.
            if (finished_count == number_of_processes) {
                finishing_time = count_time;
                is_all_finished = true;
            }

            count_time++;
        }

    }

    //
    //      Round Robin
    //
    else if (strcmp(argv[2], "rr") == 0) {
        sched_algorithm_title = "Round Robin";

        quantum = atoi(argv[3]);
        if (quantum <= 0) {
            printf("error: quantum should be greater than 0\n");
            exit(2);
        }

        // 도착 시간 순서대로 버블정렬
        for (int i = 0; i < number_of_processes; ++i) {
            for (int j = i + 1; j < number_of_processes; ++j) {
                if (process[i].A > process[j].A) {
                    pcb tmp = process[i];
                    process[i] = process[j];
                    process[j] = tmp;
                }
            }
            // 정렬된 순서대로 process id를 부여함
            process[i].pid = i;  // pid

            // RR specific
            // quantum(= time slice)만큼 time unit이 지나면 running state에 있는 프로세스를
            // ready_queue의 맨 뒤로 보내는, 즉 교체해주기 위해 remaining_quantum을 설정해줌
            process[i].remaining_quantum = quantum;
        }

        // while loop 시작 count time++
        while (!is_all_finished) {
            // process의 A(도착시각)를 체크해서 arrival time이 되면 ready queue에 enqueue
            for (int i = process_index; i < number_of_processes; ++i) {
                if (process[i].A == count_time) {
                    process[i].ready_start_time = count_time;
                    enqueue(&ready_queue, process[i]);
                    process_index++;
                }
            }

            // CPU를 1개로 가정했기 때문에 running state에는 1개의 프로세스만 온다
            //running_queue가 비어 있는 상태면 ready queue에서 프로세스를 가져옴
            if (is_empty(&running_queue)) {
                // printf("time: %d running_queue is empty\n", count_time);

                //그런데 ready_queue도 비어있는 경우에는 cpu를 활용할 수 없으므로 idle_time++
                if (is_empty(&ready_queue)) {
                    idle_time++;

                }
                // ready_queue에 process가 있는 경우 가져와서 running으로
                else {
                    pcb tmp = dequeue(&ready_queue);
                    tmp.ready_end_time = count_time;
                    tmp.waiting_time += (tmp.ready_end_time - tmp.ready_start_time);
                    enqueue(&running_queue, tmp);
                }
            }
            // running_queue에 프로세스가 있으면
            else {
                running_queue.first->remaining_cburst--;
                running_queue.first->remaining_cpu_time--;

                // quantum 만큼 시간이 지나면 교체해주기 위해
                running_queue.first->remaining_quantum--;

                // CPU time만큼 running state에 있었으면 프로세스를 종료하고 complete_queue로 보낸다
                if (running_queue.first->remaining_cpu_time == 0) {
                    pcb tmp = dequeue(&running_queue);
                    tmp.completion_time = count_time;
                    tmp.turnaround_time = tmp.completion_time - tmp.A;

                    total_turnaround_time += tmp.turnaround_time;
                    total_waiting_time += tmp.waiting_time;
                    total_blocked_time += tmp.blocked_time;

                    enqueue(&complete_queue, tmp);
                    finished_count++;
                }
                // remaining quantum이 0이 되면 remaining_quantum을 다시 quantum으로 초기화 해주고
                // ready queue로 보낸다
                else if (running_queue.first->remaining_quantum == 0) {
                    pcb tmp = dequeue(&running_queue);
                    tmp.remaining_quantum = quantum;
                    tmp.ready_start_time = count_time;
                    enqueue(&ready_queue, tmp);
                }
                // 현재 running process 의 cpu burst time이 0가 되면
                else if (running_queue.first->remaining_cburst == 0) {
                    // IO작업이 없는 프로세스일 경우 blocked process로 보내지 않고
                    // 바로 이어서 다음 작업을 시작함
                    if (running_queue.first->ioburst == 0) {
                        running_queue.first->remaining_cburst = running_queue.first->cburst;
                    }
                    // ioburst != 0 일 경우 IO작업을 하기 위해 blocked queue로 보냄
                    else {
                        pcb tmp = dequeue(&running_queue);
                        // IO작업을 위해 blocked_queue로 보낸다
                        tmp.blocked_start_time = count_time;
                        // remaining_ioburst ioburst 를 다시 초기화 해준다
                        tmp.remaining_ioburst = tmp.ioburst;
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // blocked_queue가 비어있지 않으면
            // io도 한 번에 1개의 프로세스만 io작업을 한다고 가정
            if (!is_empty(&blocked_queue)) {
                // blocked_queue에 있는 프로세스의 수만큼 for문을 돌리기 위해
                int process_count = blocked_queue.count;
                for (int i = 0; i < process_count; ++i) {
                    pcb tmp = dequeue(&blocked_queue);
                    tmp.remaining_ioburst--;

                    //block queue에 있는 io작업이 끝난 프로세스가 있으면
                    if (tmp.remaining_ioburst == 0) {
                        tmp.blocked_end_time = count_time;
                        tmp.blocked_time += (tmp.blocked_end_time - tmp.blocked_start_time);
                        tmp.remaining_cburst = tmp.cburst;
                        tmp.ready_start_time = count_time;
                        enqueue(&ready_queue, tmp);
                    }
                    // 아직 IO 작업이 남아있으면 다시 blocked_queue에 넣어준다
                    else {
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // 모든 프로세스가 완료되면 Finishing time을 기록하고 while반복문을 빠져나간다.
            if (finished_count == number_of_processes) {
                finishing_time = count_time;
                is_all_finished = true;
            }

            count_time++;
        }

    }

    //
    //  Shortest Job First (SJF)
    //
    else if (strcmp(argv[2], "sjf") == 0) {
        sched_algorithm_title = "Shortest Job First (SJF)";

        // SJF specific 정렬 : 도착한 시각 순서대로 정렬하되
        // 같은 시각에 도착했으면 C 기준으로 다시 오름차순 정렬한다
        for (int i = 0; i < number_of_processes; ++i) {
            for (int j = i + 1; j < number_of_processes; ++j) {
                // 먼저 프로세스들을 다시 C (CPU time 즉, 총 cburst의 합계) 순으로 버블 정렬
                for (int k = j; k < number_of_processes; ++k) {
                    if (process[i].C > process[j].C) {
                        pcb tmp = process[i];
                        process[i] = process[j];
                        process[j] = tmp;
                    }
                }
                // 도착한 시간 순서대로 버블정렬
                if (process[i].A > process[j].A) {
                    pcb tmp = process[i];
                    process[i] = process[j];
                    process[j] = tmp;
                }
            }
            // 정렬된 순서대로 process id를 부여함
            process[i].pid = i;  // pid
        }

        // while loop 시작 count time++
        while (!is_all_finished) {
            // process의 A(도착시각)를 체크해서 arrival time이 되면 ready queue에 enqueue
            for (int i = process_index; i < number_of_processes; ++i) {
                if (process[i].A == count_time) {
                    process[i].ready_start_time = count_time;
                    enqueue(&ready_queue, process[i]);
                    process_index++;
                }
            }

            // ready_queue에 있는 프로세스들을 remaining cpu time 순으로 정렬

            // ready queue에 있는 프로세스 수를 변수를 선언해서 저장
            int ready_queue_count = ready_queue.count;
            // ready_process 배열을 힙메모리에 할당해서 ready_queue에서 dequeue로 빼와서 저장
            pcb* ready_process = (pcb*)malloc(ready_queue_count * sizeof(pcb));
            for (int i = 0; i < ready_queue_count; ++i) {
                ready_process[i] = dequeue(&ready_queue);
            }
            // remaining_cpu_time 오름차순 순으로 버블정렬
            for (int i = 0; i < ready_queue_count; ++i) {
                for (int j = i + 1; j < ready_queue_count; ++j) {
                    if (ready_process[i].remaining_cpu_time > ready_process[j].remaining_cpu_time) {
                        pcb tmp = ready_process[i];
                        ready_process[i] = ready_process[j];
                        ready_process[j] = tmp;
                    }
                }
            }
            // 정렬된 순서로 다시 ready_queue에 넣어줌
            for (int i = 0; i < ready_queue_count; ++i) {
                enqueue(&ready_queue, ready_process[i]);
            }
            // ready_process배열의 메모리 할당 해제
            free(ready_process);
            // printf("time: %d ready_queue count: %d\n", count_time, ready_queue.count);

            // CPU가 1개로 가정했기 때문에 running state에는 1개의 프로세스만 온다
            //running_queue가 비어 있는 상태면 ready queue에서 프로세스를 가져옴
            if (is_empty(&running_queue)) {
                // printf("time: %d running_queue is empty\n", count_time);

                //그런데 ready_queue도 비어있는 경우에는 cpu를 활용할 수 없으므로 idle_time++
                if (is_empty(&ready_queue)) {
                    idle_time++;

                }
                // ready_queue에 process가 있는 경우 가져와서 running으로
                else {
                    pcb tmp = dequeue(&ready_queue);
                    tmp.ready_end_time = count_time;
                    tmp.waiting_time += (tmp.ready_end_time - tmp.ready_start_time);
                    enqueue(&running_queue, tmp);
                }
            }
            // running_queue에 프로세스가 있으면
            else {
                running_queue.first->remaining_cburst--;
                running_queue.first->remaining_cpu_time--;

                // CPU time만큼 running state에 있었으면 프로세스를 종료하고 complete_queue로 보낸다
                if (running_queue.first->remaining_cpu_time == 0) {
                    pcb tmp = dequeue(&running_queue);
                    tmp.completion_time = count_time;
                    tmp.turnaround_time = tmp.completion_time - tmp.A;

                    total_turnaround_time += tmp.turnaround_time;
                    total_waiting_time += tmp.waiting_time;
                    total_blocked_time += tmp.blocked_time;

                    enqueue(&complete_queue, tmp);
                    finished_count++;
                }
                // 현재 running process 의 cpu burst time이 0가 되면
                else if (running_queue.first->remaining_cburst == 0) {
                    // IO작업이 없는 프로세스일 경우 blocked process로 보내지 않고
                    // 바로 이어서 다음 작업을 시작함
                    if (running_queue.first->ioburst == 0) {
                        running_queue.first->remaining_cburst = running_queue.first->cburst;
                    }
                    // ioburst != 0 일 경우 IO작업을 하기 위해 blocked queue로 보냄
                    else {
                        pcb tmp = dequeue(&running_queue);
                        // IO작업을 위해 blocked_queue로 보낸다
                        tmp.blocked_start_time = count_time;
                        // remaining_ioburst ioburst 를 다시 초기화 해준다
                        tmp.remaining_ioburst = tmp.ioburst;
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // blocked_queue가 비어있지 않으면
            // io도 한 번에 1개의 프로세스만 io작업을 한다고 가정
            if (!is_empty(&blocked_queue)) {
                // blocked_queue에 있는 프로세스의 수만큼 for문을 돌리기 위해
                int process_count = blocked_queue.count;
                for (int i = 0; i < process_count; ++i) {
                    pcb tmp = dequeue(&blocked_queue);
                    tmp.remaining_ioburst--;

                    //block queue에 있는 io작업이 끝난 프로세스가 있으면
                    if (tmp.remaining_ioburst == 0) {
                        tmp.blocked_end_time = count_time;
                        tmp.blocked_time += (tmp.blocked_end_time - tmp.blocked_start_time);
                        tmp.remaining_cburst = tmp.cburst;
                        tmp.ready_start_time = count_time;
                        enqueue(&ready_queue, tmp);
                    }
                    // 아직 IO 작업이 남아있으면 다시 blocked_queue에 넣어준다
                    else {
                        enqueue(&blocked_queue, tmp);
                    }
                }
            }

            // 모든 프로세스가 완료되면 Finishing time을 기록하고 while반복문을 빠져나간다.
            if (finished_count == number_of_processes) {
                finishing_time = count_time;
                is_all_finished = true;
            }

            count_time++;
        }
    }
    // argv[2] 의 값이 제대로 주어지지 않았으면 ("fcfs", "sjf", 또는 "rr"이 아니면)
    else {
        printf("error: proivde appropriate arguments\nUsage: ./sched [filename] [fcfs, sjf, or rr]\n");
    }

    // 정상적으로 프로그램이 종료되었을 때만 출력
    if (is_all_finished == true) {
        // 결과 출력을 위한 계산식들
        avg_turnaround_time = total_turnaround_time / (double)number_of_processes;
        avg_waiting_time = total_waiting_time / (double)number_of_processes;
        throughput_per_unit = (double)number_of_processes / finishing_time;

        cpu_util = (finishing_time - idle_time) / (double)finishing_time;
        io_util = total_blocked_time / (double)finishing_time;

        // 결과 출력
        int process_print_index = complete_queue.count;
        for (int i = 0; i < process_print_index; ++i) {
            pcb tmp = dequeue(&complete_queue);
            printf(
                "-----------------------pid[%d]---------------------\n"
                "(A: %d  C: %d  B: %d  IO: %d)\n"
                "(CPU burst: %d  IO burst: %d)\n"
                "Finishing time\t\t:%8d time units\n"
                "Turnaround time\t\t:%8d time units\n"
                "CPU time\t\t:%8d time units\n"
                "IO time\t\t\t:%8d time units\n"
                "Waiting time\t\t:%8d time units\n"
                "---------------------------------------------------\n\n",
                tmp.pid,
                tmp.A, tmp.C, tmp.B, tmp.IO,
                tmp.cburst, tmp.ioburst,
                tmp.completion_time,  // Finishing time of the process
                tmp.turnaround_time,
                tmp.C,             // CPU time
                tmp.blocked_time,  // IO time
                tmp.waiting_time);
        }

        printf("\n~~~~~~~~~~~~~~~~~~~~~~~~SUMMARY~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
        printf(
            "----------------%s----------------\n"
            "Finishing time\t\t\t:%10d time units\n"
            "CPU Utilization\t\t\t:%10.1f %%\n"
            "IO Utilization\t\t\t:%10.1f %%\n"
            "Throughput per 100 time units\t:%10f processes\n"
            "Average Turnaround Time\t\t:%10.2f time units\n"
            "Average Waiting Time\t\t:%10.2f time units\n",
            sched_algorithm_title, finishing_time, cpu_util * 100, io_util * 100, throughput_per_unit * 100, avg_turnaround_time, avg_waiting_time);
        if (quantum > 0) {
            printf("Quantum for Rount Robin\t\t:%10d\n", quantum);
        }
        printf("------------------------------------------------------------\n\n");
    }

    // 메모리 할당을 해준 프로세스 구조체 배열 할당 해제
    free(process);

    return 0;
}

void queue_init(queue_t* queue) {
    queue->count = 0;
    queue->first = NULL;
    queue->last = NULL;
}

bool is_empty(queue_t* queue) {
    return (queue->count == 0);
}

void enqueue(queue_t* queue, pcb process) {
    node* n = (node*)malloc(sizeof(node));
    if (n == NULL) {
        return;
    }
    n->process = process;
    n->next = NULL;

    // queue가 비어있으면
    if (is_empty(queue)) {
        queue->first = &n->process;
        queue->last = &n->process;
    }
    //queue가 비어있지 않으면(이미 저장된 item이 있으면)
    else {
        queue->last->link = &n->process;
        queue->last = &n->process;
    }
    queue->count++;
}

pcb dequeue(queue_t* queue) {
    pcb* tmp;
    pcb process;
    // if ((queue->first == NULL) && (queue->last == NULL)) {
    if (is_empty(queue)) {
        printf("Queue is Empty!\n");
        exit(1);
    } else {
        tmp = queue->first;

        queue->first = queue->first->link;
    }
    process = *tmp;
    free(tmp);
    queue->count--;
    return process;
}
