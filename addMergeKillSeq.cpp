#include <iostream>
#include <vector>
#include <random>
#include <omp.h>

using namespace std;

#define N 1024  //total items available
#define C 33554432 //capacity of the knapsack
#define lamda 8 //parameter to determine distribution of items
#define sigma 0.001 //parameter to determine distribution of items

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

//function to implement the Add Merge Kill algorithm
vector<indexValue> addMergeKill(vector<indexValue> Si, struct Item a)
{
    vector<indexValue> Snext, tempSi;
    int j = 0, k = 0, p = 0;

    //adding the weight and value of present item
    for (auto i : Si)
    {
        indexValue inter;
        inter.index = i.index + a.weight;
        inter.value = i.value + a.value;
        tempSi.push_back(inter);
    }

    //determining the best fit entries in the row
    while (j < Si.size() && k < tempSi.size() && tempSi[k].index <= C)
    {   
        if (Si[j].index < tempSi[k].index)
        {   //storing entry with less weight first
            if (Si[j].value < tempSi[k].value)
            {   
                //storing entry with more value
                Snext.push_back(Si[j]);
                j++;
            }
            else
            {   
                //skip if value is less but weight is more
                k++;
            }
        }
        else if (Si[j].index > tempSi[k].index)
        {   
            if (Si[j].value > tempSi[k].value)
            {
                Snext.push_back(tempSi[k]);
                k++;
            }
            else
            {
                j++;
            }
        }
        else
        {   
            //if weight is same skip whichever has less weight
            if (Si[j].value > tempSi[k].value)
            {
                k++;
            }
            else
            {
                j++;
            }
        }
    }

    //adding remaining entries within the capacity
    while (j < Si.size())
    {
        Snext.push_back(Si[j]);
        j++;
    }

    while (k < tempSi.size() && tempSi[k].index <= C)
    {
        Snext.push_back(tempSi[k]);
        k++;
    }

    return Snext;
}

int main()
{
    long long int Wavg;

    Wavg = lamda * (long long int)C / (long long int)N;
    static vector<Item> bag;
    
    // generating Items list for the knapsack
    random_device rd;
    mt19937 gen(rd());
    default_random_engine generator;
    normal_distribution<float> dist(Wavg, floor(0.3 * Wavg));
    //lognormal_distribution<float> dist(Wavg,floor(0.3*Wavg));
    uniform_int_distribution<long long> dist_1(-sigma * Wavg, sigma * Wavg);
    int c_0 = 4, r;

    for (int i = 0; i < N; i++)
    {
        Item temp;
        temp.weight = floor(dist(generator));
        r = dist_1(gen);
        temp.value = c_0 * temp.weight + r;
        bag.push_back(temp);
    }

    vector<vector<indexValue>> sparse_table(2); //only two rows are sufficient for the knapsack
    indexValue temp1;
    temp1.index = 0;
    temp1.value = 0;
    sparse_table[0].push_back(temp1);

    double start_time = omp_get_wtime();

    for (int i = 1; i < N + 1; i++)
    {   
        //applying add Merge kill algorithm to get the next row
        sparse_table[i % 2] = addMergeKill(sparse_table[(i + 1) % 2], bag[i - 1]);
    }

    double end_time = omp_get_wtime();

    cout << end_time - start_time << "s\n";

}
