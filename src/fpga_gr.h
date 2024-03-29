#ifndef FPGA_GR_HPP
#define FPGA_GR_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <queue>
#include <map>
#include <cmath>
#include <climits>
#include <iomanip>
#include <time.h>
#include "node.h"

#define LIMIT_HOP 1

using namespace std;

class Channel;

class FPGA
{
public:
    int id;
    vector<pair<int, int>> nbr_pair; //nbr_fpga pairs
    map<int, int> nbr_index;         //input nbr fpga id --> output fpga index in vector nbr_pair
};

class Sink
{
public:
    int id;
    int weight;
    Sink() {}
    ~Sink() {}
};

class SubNet
{
public:
    int parent_net; //record parent net id
    int source, sink;
    int weight;
    vector<int> path;
    //map<vector<int>, >
};

class Ch_nets
{
public:
    Channel *ch; //point to channel
    //double tdm;
    //double edge_weight;
};

class Net
{
public:
    int id;
    string name;
    int source;
    double total_order; //記錄net中所有subnet 前次routing的次序index總和
    double cost;
    double max_tdm, min_tdm, total_tdm, avg_tdm; //avg_tdm--> total_tdm/#tree edges
    double total_edge_weight;
    double total_sink_weight;
    double criticality;
    bool sorted;
    bool ripped[5];

    vector<Sink> sink;
    vector<SubNet> sbnet;
    vector<pair<int, int>> channels;
    Tree_Node *rtree_root; //routing tree root
    map<pair<int, int>, int> edge_crit;
    map<pair<int, int>, double> chan_penalty;

    int total_tree_edge; //record # of tree edge
    double signal_weight;
    vector<pair<vector<int>, SubNet>> allpaths;

    void net_initialize()
    {
        total_tree_edge = 0;
        total_tdm = 0.0;
        total_edge_weight = 0.0;
        max_tdm = 0.0;
        min_tdm = INT_MAX;
    }

    Net()
    {
        sorted = false;
        rtree_root = NULL;
        total_tree_edge = 0;
        total_tdm = 0.0;
        total_edge_weight = 0.0;
        total_sink_weight = 0.0;
        max_tdm = 0.0;
        min_tdm = INT_MAX;
    }
};

class Channel
{
public:
    pair<int, int> name;
    list<pair<Net *, double>> net_ch_weight; //net list and edge weight
    double history_used[2];                  // min-->max : index=0
    double history_cost[2];                  //for CCR
    double history_penalty[2];               //for CCR
    int capacity;
    list<Net *> passed_nets[2]; //紀錄經過的Net

    Channel()
    {
        capacity = 0;
        history_used[0] = history_used[1] = 0.0;
        history_cost[0] = history_cost[1] = 0.0;
        history_penalty[0] = history_penalty[1] = 1.0;
    }

    ~Channel();
};

class Table_content
{
public:
    int hops;
    vector<int> parent;
    Table_content()
    {
        hops = 0;
    }
};

class Path_table_ver2
{
public:
    vector<Table_content> cand;
};

class FPGA_Gr
{
public:
    int round;
    int fpga_num;
    int sink_num;
    int total_demand;
    int capacity;
    double total_cost, avg_sk_weight;
    double avg_tdm_ratio;
    int mintdm, maxtdm;
    double maxsgw, minsgw; // max and min signal weight
    bool subnetbased;

    vector<FPGA> fpga;
    vector<Net> net;
    vector<SubNet> subnet;
    vector<vector<Path_table_ver2>> path_table_ver2;
    map<pair<int, int>, int> channel_demand; //2 fpga --> demand signals
    map<pair<int, int>, Channel *> map_to_channel;
    map<pair<int, int>, int> channel_capacity; //2 fpga --> channel capacity
    map<pair<int, int>, int> channel_total_edge_weight;
    map<pair<int, int>, int> congestion_map;
    vector<pair<pair<int, int>, int>> cong_map_vec;
    map<pair<int, int>, int> old_map_vec;
    map<pair<int, int>, int> RRtimes;
    vector<int> after_conj_cost;
    vector<int> after_total_weight;

    //2020/08/31 統計重複RR個數
    vector<bool> repeat_RR;

    //2020/07/19
    pair<int, int> top1_tdm_channel;
    pair<int, int> top2_tdm_channel;
    pair<int, int> top3_tdm_channel;
    pair<int, int> top4_tdm_channel;
    pair<int, int> top5_tdm_channel;

    FPGA_Gr()
    {
        round = 1;
        sink_num = total_cost = total_demand = 0;
        maxsgw = 0;
        maxtdm = 0;
        minsgw = mintdm = INT_MAX;
        subnetbased = false;
    }
    ~FPGA_Gr() {}
    
    void getfile(char *, char *);
    void breakdown(); //break down all net into 2 pin subnet
    void construct_table();
    void show_path_table();
    void add_channel_demand(const int &, const int &);
    void sub_channel_demand(const int &, const int &);
    void global_routing();
    void routing_tree(Net &, const vector<vector<int>> &);
    void compute_edge_weight(Net &, Tree_Node *);
    bool find_rip_up_edge(Net &, int &, int &); //input net root and get edge with parent and child
    Tree_Node *rip_up_edge(Net &, const int &, const int &);
    void reroute_edge(Net &, const int &, Tree_Node *, double);
    void rip_up_reroute(time_t);
    int channel_used(int, int);
    double channel_TDM(int, int);
    double compute_TDM_cost();
    int ret_channel_capacity(const int & , const int &);
    double comptue_tree_TDM_cost(Tree_Node *);
    void record_net_channel_used();
    void show_net_channel_table();
    void output_file(char *output, time_t);
    void check_result();

    //another global routing
    void construct_table_ver2(); //考慮hops數多1~2的可能
    void global_routing_ver2();  //考慮tdm(orcd  congestion)
    void show_path_table_ver2();
    double compute_cost_for_gr2(Net &, const vector<int> &, const SubNet &, int &sink_num);

    //history cost 2020/03/29
    void initial_route_result();

    void global_routing_ver3();
    void routing_subtree(Net &, const vector<int> &);

    //channel direct 2020/04/08
    //void distribute_channel_capacity(); //依比例分配channel的capacity

    //2020/06/21
    void max_subpath_RR();
    vector<pair<int, int>> sub_allchannels(Net &n, Tree_Node *sbtree_root, vector<SubNet> &allsubnets, map<int, int> &all_rip_nodes);

    //2020/07/11
    void subtree_sink_RR(); //挑出subtree root把subtree內的sink都用cost function重繞
    void subtree_RR();      //挑出subtree root，把他連到別的點

    //2020/07/19
    void show_congestion_map(); //印出Channel各方向的使用量、TDM分配以及有哪些訊號通過
    void congestion_RR();
    void rip_up_net(Net &n);
    void reroute_net(Net *n);
    double compute_cost_for_CCR(Net &, const vector<int> &, const SubNet &, int &sink_num);
    void update_history_cost();

    //2020/08/11
    void set_after_conj_cost();

    //2020/09/01
    void add_ch_RRtimes(pair<int, int>);
};

#endif