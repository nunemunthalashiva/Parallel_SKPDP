#include <iostream>
#include <vector>
#include <random>
#include <deque>
#include <algorithm>
#include <omp.h>

#define threads 8  //threads used in the program
#define buffer_size 8000 //size of intermediate buffers
#define N 1024 //items available
#define C 33554432 // capacity of the knapsack
#define lamda 8 //parameter to determine distribution of items
#define sigma 0.001 //parameter to determine distribution of items

using namespace std;

//to store a entry in a row of the SKPDP table
struct indexValue
{
    long long int index, value;
};

//to store details of an item
struct Item
{
    long long int weight, value;
};

int main()
{
    long long int Wavg;
    Wavg = lamda * (long long int)C / (long long int)N;
    vector<Item> bag;

    // generating Items list for the knapsack
    random_device rd;
    mt19937 gen(rd());
    default_random_engine generator;
    normal_distribution<float> dist(Wavg, floor(0.3 * Wavg));
    //lognormal_distribution<float> dist(Wavg,floor(0.3*Wavg));
    uniform_int_distribution<long long> dist_1(-sigma * Wavg, sigma * Wavg);
    int c_0 = 4, r;

    for (long long int i = 0; i < N; i++)
    {
        Item temp;
        temp.weight = floor(dist(generator));
        r = dist_1(gen);
        temp.value = c_0 * temp.weight + r;
        bag.push_back(temp);
    }

    vector<indexValue> present_row; // to store the row from which first thread takes the input
    vector<indexValue> final_row; // to store the row to which last thread stores output
    indexValue temp; //intialising first entry with <0,0> 0 weight and 0 value
    temp.index = 0;
    temp.value = 0;
    present_row.push_back(temp);

    //locks to synchronise the use of buffers among the threads
    omp_lock_t locks[threads];
    for (int i = 0; i < threads; i++)
    {
        omp_init_lock(&locks[i]);
    }

    double start_time = omp_get_wtime();

    //doing a set of rows parllel at a time
    for (int i = 0; i < N; i += threads)
    {
         // to keep track whether a thread completed doing its work
        vector<bool> complete(threads, false);
        //internal buffers to share data
        vector<deque<indexValue>> inter_buffers(threads);

        //cout<<present_row.size()<<"\n";

        #pragma omp parallel num_threads(threads)
        {
            int thread_id = omp_get_thread_num();

            //local buffers of a thread to facilitate add merge kill
            deque<indexValue> tempStore1, tempStore2;
            

            if (thread_id == 0)
            {
                //first thread which takes input from present_row
                int k = 0;
                long long int j = 0, p = 0;

                //loop until all elements in present_row consumed and output is generated from it
                while (j < present_row.size())
                {
                    //consuming a chunk from present_row
                    while (j - p < buffer_size && j < present_row.size())
                    {
                        indexValue temp1;
                        temp1.index = present_row[j].index + bag[i].weight;
                        temp1.value = present_row[j].value + bag[i].value;
                        tempStore2.push_back(temp1); //storing the entry if the item is added to knapsack
                        j++;
                    }

                    //generating and storing the output in the buffer
                    if (inter_buffers[0].size() < buffer_size && p < j)
                    {
                        //lock for inter_buffers[0]
                        omp_set_lock(&locks[0]);
                        //generate and store output until buffer is full using Add Merge Kill
                        while (inter_buffers[0].size() < buffer_size && p < j && !tempStore2.empty())
                        {
                            if (present_row[p].index < tempStore2.front().index)
                            {
                                if (present_row[p].value < tempStore2.front().value)
                                {
                                    inter_buffers[0].push_back(present_row[p]);
                                    p++;
                                }
                                else
                                {
                                    tempStore2.pop_front();
                                }
                            }
                            else if (present_row[p].index > tempStore2.front().index)
                            {
                                if (present_row[p].value > tempStore2.front().value)
                                {
                                    if (tempStore2.front().index <= C)
                                        inter_buffers[0].push_back(tempStore2.front());
                                    tempStore2.pop_front();
                                }
                                else
                                {
                                    p++;
                                }
                            }
                            else
                            {
                                if (present_row[p].value > tempStore2.front().value)
                                {
                                    tempStore2.pop_front();
                                }
                                else
                                {
                                    p++;
                                }
                            }
                        }

                        omp_unset_lock(&locks[0]);
                    }
                }
                
                 omp_set_lock(&locks[0]);
                //store the remaining chunk which is read from present_row remained after tempStore2 is emptied
                while (inter_buffers[0].size() < buffer_size && p < j)
                {
                            inter_buffers[0].push_back(present_row[p]);
                            p++;
                }
                
                //storing all items in tempStore2 which remained and weight within capacity
                while (inter_buffers[0].size() < buffer_size && !tempStore2.empty())
                {
                    if (tempStore2.front().index <= C)
                        inter_buffers[0].push_back(tempStore2.front());
                    tempStore2.pop_front();
                }
                omp_unset_lock(&locks[0]);

                //1st thread completed its work
                complete[0] = true;
            }
            else if (thread_id == threads - 1)
            {   
                //last thread

                while (1)
                {
                    bool comp = complete[threads - 2]; //checking whether previous thread completed

                    if (!inter_buffers[threads - 2].empty())
                    {   
                        //if inter_buffers[threads-2] has data to read then set lock to consume it
                        omp_set_lock(&locks[threads - 2]);
                        while (!inter_buffers[threads - 2].empty())
                        {
                            indexValue temp1;
                            temp1.index = inter_buffers[threads - 2].front().index + bag[i + threads - 1].weight;
                            temp1.value = inter_buffers[threads - 2].front().value + bag[i + threads - 1].value;
                            tempStore1.push_back(inter_buffers[threads - 2].front()); // storing the exact data in tempStore1
                            tempStore2.push_back(temp1); // storing the data with adding the item in tempStore2
                            inter_buffers[threads - 2].pop_front(); //removing the consumed data
                        }
                        omp_unset_lock(&locks[threads - 2]);
                    }

                    //perform Add Merge Kill to tempStore1 and tempStore2 and store the output in final_row
                    while (!tempStore1.empty() && !tempStore2.empty())
                    {
                        if (tempStore1.front().index < tempStore2.front().index)
                        {
                            if (tempStore1.front().value < tempStore2.front().value)
                            {
                                final_row.push_back(tempStore1.front());
                                tempStore1.pop_front();
                            }
                            else
                            {
                                tempStore2.pop_front();
                            }
                        }
                        else if (tempStore1.front().index > tempStore2.front().index)
                        {
                            if (tempStore1.front().value > tempStore2.front().value)
                            {
                                if (tempStore2.front().index <= C)
                                    final_row.push_back(tempStore2.front());
                                tempStore2.pop_front();
                            }
                            else
                            {
                                tempStore1.pop_front();
                            }
                        }
                        else
                        {
                            if (tempStore1.front().value > tempStore2.front().value)
                            {
                                tempStore1.pop_front();
                            }
                            else
                            {
                                tempStore2.pop_front();
                            }
                        }
                    }

                    if (comp)
                    {   
                        
                        //storing the remaining values in tempStore1
                    while (!tempStore1.empty())
                    {
                        final_row.push_back(tempStore1.front());
                        tempStore1.pop_front();
                    }


                        //if reading is completed and previous thread completed execution
                        //store the remaining values if the weight within capacity
                        while (!tempStore2.empty())
                        {
                            if (tempStore2.front().index <= C)
                                final_row.push_back(tempStore2.front());
                            tempStore2.pop_front();
                        }
                        //thread completed execution
                        break;
                    }
                }
            }
            else
            {

                while (1)
                {
                    bool comp = complete[thread_id - 1]; //previous thread execution status

                    if (!inter_buffers[thread_id - 1].empty())
                    {   
                        //trying to consume if inter_buffers[thread_id-1] is not empty
                        omp_set_lock(&locks[thread_id - 1]);
                        while (!inter_buffers[thread_id - 1].empty())
                        {
                            indexValue temp1;
                            temp1.index = inter_buffers[thread_id - 1].front().index + bag[i + thread_id].weight;
                            temp1.value = inter_buffers[thread_id - 1].front().value + bag[i + thread_id].value;
                            tempStore1.push_back(inter_buffers[thread_id - 1].front()); // storing the exact data in tempStore1
                            tempStore2.push_back(temp1); // storing the data with adding the item in tempStore2
                            inter_buffers[thread_id - 1].pop_front(); //removing the consumed data
                        }
                        omp_unset_lock(&locks[thread_id - 1]);
                    }

                    //write output if inter_buffers[thread_id] is not full
                    if (inter_buffers[thread_id].size() < buffer_size && !tempStore1.empty() && !tempStore2.empty())
                    {   
                        //trying to write output if inter_buffers[thread_id] is not full
                        omp_set_lock(&locks[thread_id]);
                        // use Add Merge Kill to produce output
                        while (inter_buffers[thread_id].size() < buffer_size && !tempStore1.empty() && !tempStore2.empty())
                        {
                            if (tempStore1.front().index < tempStore2.front().index)
                            {
                                if (tempStore1.front().value < tempStore2.front().value)
                                {
                                    inter_buffers[thread_id].push_back(tempStore1.front());
                                    tempStore1.pop_front();
                                }
                                else
                                {
                                    tempStore2.pop_front();
                                }
                            }
                            else if (tempStore1.front().index > tempStore2.front().index)
                            {
                                if (tempStore1.front().value > tempStore2.front().value)
                                {
                                    if (tempStore2.front().index <= C)
                                        inter_buffers[thread_id].push_back(tempStore2.front());
                                    tempStore2.pop_front();
                                }
                                else
                                {
                                    tempStore1.pop_front();
                                }
                            }
                            else
                            {
                                if (tempStore1.front().value > tempStore2.front().value)
                                {
                                    tempStore1.pop_front();
                                }
                                else
                                {
                                    tempStore2.pop_front();
                                }
                            }
                        }

                        omp_unset_lock(&locks[thread_id]);
                    }
                    
                    //if previous thread completed exection and one of tempStore1 and tempStor2 is empty
                    if (comp && (tempStore1.empty() || tempStore2.empty()))
                    {   
                        //trying to write the remaining items in tempStore2
                        omp_set_lock(&locks[thread_id]);

                        //storing the remaining values in tempStore1 to inter_buffers[thread_id]
                        while (inter_buffers[thread_id].size() < buffer_size && !tempStore1.empty())
                        {
                            inter_buffers[thread_id].push_back(tempStore1.front());
                            tempStore1.pop_front();
                        }

                        while (inter_buffers[thread_id].size() < buffer_size && !tempStore2.empty())
                        {
                            if (tempStore2.front().index <= C)
                                inter_buffers[thread_id].push_back(tempStore2.front());
                            tempStore2.pop_front();
                        }
                        omp_unset_lock(&locks[thread_id]);

                        if (tempStore1.empty() && tempStore2.empty())
                        {
                            complete[thread_id] = true;
                            //marking completion of this thread
                            break;
                        }
                    }
                }
            }

        }
        present_row = final_row; //storing the values in present_row for next set of rows
        final_row.clear(); //clearing for next set of rows

    }

    double end_time = omp_get_wtime();

    cout << end_time - start_time << "s\n";

}
