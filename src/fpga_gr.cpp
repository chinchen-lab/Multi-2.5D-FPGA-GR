#include "fpga_gr.h"

bool comp_weight(const SubNet &lhs, const SubNet &rhs)
{
    return lhs.weight > rhs.weight;
}

bool comp_sbnetcost(const pair<SubNet, int> &lhs, const pair<SubNet, int> &rhs)
{
    return lhs.second > rhs.second;
}

bool comp_netcrit(const Net &lhs, const Net &rhs)
{
    return lhs.criticality > rhs.criticality;
}

bool comp_netsgw(const Net &lhs, const Net &rhs)
{
    return lhs.signal_weight / (double)lhs.sink.size() > rhs.signal_weight / (double)rhs.sink.size();
}

bool comp_edges(const Net &lhs, const Net &rhs)
{
    return lhs.total_tree_edge > rhs.total_tree_edge;
}

bool comp_hops(const Table_content &lhs, const Table_content &rhs)
{
    return lhs.hops < rhs.hops;
}

bool comp_by_second(const pair<Net *, double> &lhs, const pair<Net *, double> &rhs)
{
    return lhs.second > rhs.second;
}

bool comp_conj_vec_map(const pair<pair<int, int>, int> &lhs, const pair<pair<int, int>, int> &rhs)
{
    return lhs.second > rhs.second;
}

void show_tree(Tree_Node *root)
{
    if (root == NULL)
    {
        cout << "root is null !" << endl;
        return;
    }

    cout << "source fpga : " << root->fpga_id << endl;
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    cout << "parent    child\n";
    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->children.size() == 0)
            continue;

        cout << setw(3) << cur->fpga_id << "        ";
        for (const auto &child : cur->children)
        {
            cout << setw(3) << child->fpga_id << " ";
            fifo_queue.push(child);
        }
        cout << endl;
    }

    //show all node's max value and sink weight
    fifo_queue.push(root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        cout << "F" << cur->fpga_id << " : " << cur->sink_weight << " " << cur->max_value << endl;
        for (const auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }
}

void delete_tree(Tree_Node *root)
{
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->children.size() == 0)
            continue;

        for (const auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
        delete cur;
    }
}

int max_weight(Tree_Node *root) //return max sink weight value of all subtree
{
    int max = 0;
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        for (auto &child : cur->children)
        {
            if (child->sink_weight > max)
            {
                max = child->sink_weight;
            }

            fifo_queue.push(child);
        }
    }

    return max;
}

double diff_weight_es(Tree_Node *root) //return tree的edge weight平均 與 sink weight平均 的 差
{
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    double total_s, total_e, avg_s, avg_e, sink_num, edge_num;
    total_e = total_s = sink_num = edge_num = 0.0;

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->sink_weight != 0)
        {
            sink_num++;
            total_s += cur->sink_weight;
        }

        if (cur->fpga_id != root->fpga_id)
        {
            edge_num++;
            total_e += cur->max_value;
        }

        for (auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }

    avg_e = total_e / edge_num;
    avg_s = total_s / sink_num;

    //cout << total_e << " " << total_s << endl;
    //cout << edge_num << " " << sink_num << endl;

    return (avg_e - avg_s);
}

pair<int, int> get_channel_name(int x, int y)
{
    int s = min(x, y);
    int t = max(x, y);
    return make_pair(s, t);
}

Tree_Node *search_node(Tree_Node *root, int key)
{
    Tree_Node *current = root;

    if (current->fpga_id == key)
    {
        return current;
    }

    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(current);

    while (fifo_queue.size() != 0 && current != NULL && current->fpga_id != key)
    {
        current = fifo_queue.front();
        fifo_queue.pop();

        if (current->children.size() != 0)
        {
            for (auto child : current->children)
            {
                if (child->fpga_id != key)
                {
                    fifo_queue.push(child);
                }
                else
                {
                    return child;
                }
            }
        }
    }

    return nullptr;
}

void FPGA_Gr::getfile(char *sysfile, char *netfile)
{
    vector<int> fpgas, fpga_nbr, pair;
    FILE *fr;
    int f1, f2;
    int fu1, fm1, ff1, fu2, fm2, ff2, pairs;
    fr = fopen(sysfile, "r");

    while (!feof(fr))
    {
        int mak = fscanf(fr, "U%d/M%d/F%d[SLR%*d]---U%d/M%d/F%d[SLR%*d], pairs=%d\n", &fu1, &fm1, &ff1, &fu2, &fm2, &ff2, &pairs);
        f1 = 100 * fu1 + 10 * fm1 + 1 * ff1;
        f2 = 100 * fu2 + 10 * fm2 + 1 * ff2;
        fpgas.push_back(f1);
        fpga_nbr.push_back(f2);
        pair.push_back(pairs);
    }

    fclose(fr);

    fpga_num = fpgas.back() + 1;
    fpga.resize(fpga_num);

    for (int i = 0; i < fpgas.size(); i++)
    {
        int id = fpgas[i];
        int nbr_id = fpga_nbr[i];
        pairs = pair[i];

        fpga[id].id = id;

        if (fpga[id].nbr_pair.size() == 0)
        {
            fpga[id].nbr_pair.push_back(make_pair(nbr_id, pairs));
        }
        else if (nbr_id != fpga[id].nbr_pair.back().first)
        {
            fpga[id].nbr_pair.push_back(make_pair(nbr_id, pairs));
        }
        else
        {
            fpga[id].nbr_pair.back().second += pairs;
        }
    }

    int interconnect = 0;
    for (auto &f : fpga) //record index
    {
        for (size_t i = 0; i < f.nbr_pair.size(); i++)
        {
            const int &fid = f.nbr_pair[i].first;
            f.nbr_index[fid] = i;
            interconnect++;
        }
    }

    interconnect /= 2;

    cout << endl;
    cout << "#FPGAs = " << fpga.size() << endl;
    cout << "#interconnections = " << interconnect << endl;

    /*
    for (auto &f : fpga)
    {
        for (auto &nbr_f : f.nbr_pair)
        {
            //cout << "F" << f.id << "-------"
                 << "F" << nbr_f.first << ", pairs = " << nbr_f.second << endl;
        }
    }
    */

    //read net
    fstream file;
    file.open(netfile);
    string line;
    int id = 0;
    double total_sink_weight = 0.0;

    while (getline(file, line, '\n'))
    {
        Net n;
        n.id = id++;
        istringstream templine(line);
        string data;
        int ctrl = 0;
        bool src = false;
        vector<int> tc;
        vector<int> t_weight;

        while (getline(templine, data, ','))
        {
            int current = 0;
            int pos = data.find_first_of("_", current);
            if (pos > 0)
            {
                n.name = data;
                ctrl = 1;
            }
            else if (ctrl == 1)
            {
                t_weight.push_back(atoi(data.c_str()));
            }
            else
            {
                if (!src) //還沒讀source
                {
                    n.source = atoi(data.c_str());
                    src = true;
                }
                else
                {
                    Sink tmp_s;
                    tmp_s.id = atoi(data.c_str());
                    n.sink.push_back(tmp_s);
                }
            }
        }

        for (size_t i = 0; i < n.sink.size(); i++)
        {
            n.sink[i].weight = t_weight[i];
            n.total_sink_weight += t_weight[i];
            total_sink_weight += t_weight[i];
            sink_num++;
        }

        net.push_back(n);
    }

    avg_sk_weight = total_sink_weight / (double)sink_num;

    capacity = interconnect % 100 * 1000 / fpga_num % 10 * 10;

    cout << "#signals = " << net.size() << endl;
    cout << "capacity = " << interconnect % 100 * 1000 / fpga_num % 10 * 10 << endl;
    cout << endl;

    repeat_RR.resize(net.size());

    /*
    for (auto &n : net)
    {
        //cout << n.name << endl;
        //cout << "source = " << n.source << endl;
        for (auto &s : n.sink)
        {
            //cout << s.id << ", w = " << s.weight << endl;
        }
        //cout << endl;
    }
    */

    //srand(time(NULL));
    //random_shuffle(net.begin(), net.end());
}

void FPGA_Gr::output_file(char *outfile, time_t t)
{
    ofstream myresult(outfile);
    myresult << "cost = " << fixed << setprecision(0) << total_cost << endl;
    myresult << "runtime = " << (double)(clock() - t) / (double)CLOCKS_PER_SEC << " seconds\n";
}

void FPGA_Gr::breakdown()
{
    for (auto &n : net)
    {
        const int &source = n.source;
        for (auto &sink : n.sink)
        {
            SubNet tmp;
            tmp.parent_net = n.id;
            tmp.source = source;
            tmp.sink = sink.id;
            tmp.weight = sink.weight;
            n.sbnet.push_back(tmp);
        }
    }

    //print break down subnet
    /*
    for (auto &n : net)
    {
        //cout << n.name << endl;
        for (auto &sbn : n.sbnet)
        {
            //cout << "   " << sbn.source << " " << sbn.sink << ", weight = " << sbn.weight << endl;
        }
    }
    */
}

void FPGA_Gr::construct_table_ver2()
{
    //resize path table
    path_table_ver2.resize(fpga_num);
    for (auto &pt : path_table_ver2)
        pt.resize(fpga_num);

    int k = LIMIT_HOP;     //限制最多超過min_hop k
    int sol_num_limit = 5; //最多存幾條路徑

    for (int i = 0; i < fpga_num; i++)
    {
        queue<vector<int>> init_queue;
        queue<vector<int>> path_queue;

        for (const auto &nbr : fpga[i].nbr_pair)
        {
            const int &nbr_fpga = nbr.first;
            vector<int> init_path;
            init_path.push_back(i);
            init_path.push_back(nbr_fpga);
            init_queue.push(init_path);
        }

        //search fpga_i to fpga_j min ~ min + k hops
        for (int j = i + 1; j < fpga_num; j++)
        {
            vector<vector<int>> path_sol;
            path_queue = init_queue;
            int min_hop = INT_MAX;
            int path_sol_num = 0;
            //cout << "search F" << i << " to F" << j << " paths..." << endl;

            while (!path_queue.empty())
            {
                bool find = false;
                auto cur_path = path_queue.front();
                path_queue.pop();

                if (cur_path.size() - 1 > min_hop + k)
                {
                    continue;
                }

                const int &cur_fpga = cur_path.back();

                if (cur_fpga == j) //find target
                {
                    find = true;
                    path_sol_num++;

                    if (cur_path.size() - 1 < min_hop)
                    {
                        min_hop = cur_path.size() - 1;
                    }

                    //存入path table
                    const int &start = cur_path[0];

                    bool flag1 = false; //看看是否已有相同hop數的解
                    bool flag2 = false;

                    for (auto &pt : path_table_ver2[i][j].cand)
                    {
                        if (pt.hops == cur_path.size() - 1)
                        {
                            flag1 = true;
                            const int &par = cur_path[cur_path.size() - 2];

                            bool isAdded = false;
                            for (const auto &added : pt.parent)
                            {
                                if (added == par)
                                {
                                    isAdded = true;
                                    break;
                                }
                            }

                            if (!isAdded)
                            {
                                pt.parent.push_back(par);
                            }

                            break;
                        }
                    }

                    for (auto &pt : path_table_ver2[j][i].cand)
                    {
                        if (pt.hops == cur_path.size() - 1)
                        {
                            flag2 = true;
                            const int &par = cur_path[1];

                            bool isAdded = false;
                            for (const auto &added : pt.parent)
                            {
                                if (added == par)
                                {
                                    isAdded = true;
                                    break;
                                }
                            }

                            if (!isAdded)
                            {
                                pt.parent.push_back(par);
                            }
                            break;
                        }
                    }

                    if (flag1 == false)
                    {
                        Table_content temp;
                        temp.hops = cur_path.size() - 1;
                        temp.parent.push_back(cur_path[cur_path.size() - 2]);
                        path_table_ver2[i][j].cand.push_back(temp);
                    }

                    if (flag2 == false)
                    {
                        Table_content temp;
                        temp.hops = cur_path.size() - 1;
                        temp.parent.push_back(cur_path[1]);
                        path_table_ver2[j][i].cand.push_back(temp);
                    }
                }

                if (path_sol_num > sol_num_limit)
                {
                    break;
                }

                if (!find)
                {
                    vector<int> visited(fpga_num, 0);
                    for (const auto &fid : cur_path)
                    {
                        visited[fid] = 1;
                    }

                    //往下走一步
                    for (const auto &nbr : fpga[cur_fpga].nbr_pair)
                    {
                        if (visited[nbr.first] == 0)
                        {
                            auto temp_path = cur_path;
                            temp_path.push_back(nbr.first);

                            if (nbr.first == j) //確認是否find target
                            {
                                find = true;
                                path_sol_num++;

                                if (temp_path.size() - 1 < min_hop)
                                {
                                    min_hop = temp_path.size() - 1;
                                }

                                //存入path table
                                const int &start = temp_path[0];

                                bool flag1 = false; //看看是否已有相同hop數的解
                                bool flag2 = false;

                                for (auto &pt : path_table_ver2[i][j].cand)
                                {
                                    if (pt.hops == temp_path.size() - 1)
                                    {
                                        flag1 = true;
                                        const int &par = temp_path[temp_path.size() - 2];

                                        bool isAdded = false;
                                        for (const auto &added : pt.parent)
                                        {
                                            if (added == par)
                                            {
                                                isAdded = true;
                                                break;
                                            }
                                        }

                                        if (!isAdded)
                                        {
                                            pt.parent.push_back(par);
                                        }

                                        break;
                                    }
                                }

                                for (auto &pt : path_table_ver2[j][i].cand)
                                {
                                    if (pt.hops == temp_path.size() - 1)
                                    {
                                        flag2 = true;
                                        const int &par = temp_path[1];

                                        bool isAdded = false;
                                        for (const auto &added : pt.parent)
                                        {
                                            if (added == par)
                                            {
                                                isAdded = true;
                                                break;
                                            }
                                        }

                                        if (!isAdded)
                                        {
                                            pt.parent.push_back(par);
                                        }
                                        break;
                                    }
                                }

                                if (flag1 == false)
                                {
                                    Table_content temp;
                                    temp.hops = temp_path.size() - 1;
                                    temp.parent.push_back(temp_path[temp_path.size() - 2]);
                                    path_table_ver2[i][j].cand.push_back(temp);
                                }

                                if (flag2 == false)
                                {
                                    Table_content temp;
                                    temp.hops = temp_path.size() - 1;
                                    temp.parent.push_back(temp_path[1]);
                                    path_table_ver2[j][i].cand.push_back(temp);
                                }
                            }
                            else //not find target
                            {
                                path_queue.push(temp_path);
                            }
                        }
                    }
                }
            }
        }
    }

    //sort all candidate path by hops
    for (int i = 0; i < fpga_num; i++)
    {
        for (int j = 0; j < fpga_num; j++)
        {
            if (i != j)
                sort(path_table_ver2[i][j].cand.begin(), path_table_ver2[i][j].cand.end(), comp_hops);
        }
    }

    //construct channel table
    for (size_t i = 0; i < fpga.size(); i++)
    {
        for (size_t j = i + 1; j < fpga.size(); j++)
        {
            pair<int, int> chan = make_pair(i, j);
            Channel *ch = new Channel();
            ch->name = chan;
            map_to_channel[chan] = ch;
        }
    }

    //record capacity
    for (int i = 0; i < fpga_num; i++)
    {
        for (size_t j = 0; j < fpga[i].nbr_pair.size(); j++)
        {
            int nbr_id = fpga[i].nbr_pair[j].first;
            auto ch_name = get_channel_name(i, nbr_id);
            auto ch = map_to_channel[ch_name];
            //ch->capacity = fpga[i].nbr_pair[j].second;
            ch->capacity = capacity;
        }
    }

    /*
    for (const auto &ch:map_to_channel)
    {
        if(ch.second->capacity == 0)
            continue;
        
        cout << "channel (" << ch.first.first << ", " << ch.first.second << ") : " << endl;
        cout << "capacity = " << ch.second->capacity << endl;
        cout << endl;
    }
    */

    cout << "OK" << endl;
    //show_path_table_ver2();
}

void FPGA_Gr::show_path_table_ver2()
{
    for (int i = 0; i < fpga_num; i++)
    {
        for (int j = 0; j < fpga_num; j++)
        {
            if (i == j)
                continue;

            cout << "F" << i << " to "
                 << "F" << j << " : " << endl;

            for (const auto &pt2 : path_table_ver2[i][j].cand)
            {
                cout << "hops = " << pt2.hops << endl;
                cout << "parents : ";
                for (const auto &par : pt2.parent)
                {
                    cout << par << " ";
                }
                cout << "\n";
            }
            cout << endl;
        }
    }
}

void FPGA_Gr::add_channel_demand(const int &s, const int &t)
{
    auto ch_name = get_channel_name(s, t);
    auto ch = map_to_channel[ch_name];
    int ch_cap = ch->capacity;
    double dir_0 = ++channel_demand[make_pair(s, t)];
    double dir_1 = channel_demand[make_pair(t, s)];
    double total = dir_0 + dir_1;
    int cap_0 = channel_capacity[make_pair(s, t)] = (double)ch_cap * (dir_0 / total);
    int cap_1 = channel_capacity[make_pair(t, s)] = ch_cap - cap_0;

    if (cap_0 == 0 && dir_0 != 0)
    {
        channel_capacity[make_pair(s, t)]++;
        channel_capacity[make_pair(t, s)]--;
    }

    if (cap_1 == 0 && dir_1 != 0)
    {
        channel_capacity[make_pair(s, t)]--;
        channel_capacity[make_pair(t, s)]++;
    }

    total_demand++;
}

void FPGA_Gr::sub_channel_demand(const int &s, const int &t)
{
    auto ch_name = get_channel_name(s, t);
    auto ch = map_to_channel[ch_name];
    int ch_cap = ch->capacity;
    double dir_0 = --channel_demand[make_pair(s, t)];
    double dir_1 = channel_demand[make_pair(t, s)];
    double total = dir_0 + dir_1;
    int cap_0 = channel_capacity[make_pair(s, t)] = (double)ch_cap * (dir_0 / total);
    int cap_1 = channel_capacity[make_pair(t, s)] = ch_cap - cap_0;

    if (cap_0 == 0 && dir_0 != 0)
    {
        channel_capacity[make_pair(s, t)]++;
        channel_capacity[make_pair(t, s)]--;
    }

    if (cap_1 == 0 && dir_1 != 0)
    {
        channel_capacity[make_pair(s, t)]--;
        channel_capacity[make_pair(t, s)]++;
    }

    total_demand--;
}

int FPGA_Gr::ret_channel_capacity(const int &s, const int &t)
{
    return channel_capacity[make_pair(s, t)];
}

void FPGA_Gr::global_routing_ver2()
{
    int routed_net = 0;
    int hop_limit = LIMIT_HOP; //最多嘗試與最小hop數差幾個hop的限制條件

    //srand(time(NULL));

    /*if (round > 1)
    {
        sort(net.begin(), net.end(), comp_netsgw);
    }*/

    for (auto &n : net)
    {
        //cout << n.name << " all subnets' paths" << endl;

        map<pair<int, int>, int> edge_lut; //使用過的edge會加入這個map

        if (!n.sorted)
        {
            sort(n.sbnet.begin(), n.sbnet.end(), comp_weight);
            n.sorted = true;
        }

        vector<int> sources;
        vector<vector<int>> allpaths; //record all subnet's path

        for (auto &sb : n.sbnet)
        {
            //cout << "route sink " << sb.sink << endl;

            if (sources.size() == 0) //route net's first subnet
            {
                vector<int> path; //sink->source path
                path.push_back(sb.sink);
                sources.push_back(sb.sink);

                int cand_hop_num = path_table_ver2[sb.source][sb.sink].cand.size();
                //cout << "candidate hop number = " << cand_hop_num << endl;
                //cout << "source = " << sb.source << ", sink = " << sb.sink << endl;
                if (cand_hop_num == 1) //一般作法
                {
                    int min_hops = path_table_ver2[sb.source][sb.sink].cand[0].hops;

                    if (min_hops == 1) //hops = 1 --> done !
                    {
                        path.push_back(sb.source);
                        sources.push_back(sb.source);
                        allpaths.push_back(path);

                        if (edge_lut.count(make_pair(sb.source, sb.sink)) == 0) //edge沒出現過
                        {
                            edge_lut[make_pair(sb.source, sb.sink)] = 1;
                            add_channel_demand(sb.source, sb.sink);
                        }

                        continue;
                    }

                    vector<vector<int>> cand_path; //save all candidate path
                    const int &sc = sb.source;

                    queue<vector<int>> path_queue;
                    for (const auto &first_parent : path_table_ver2[sc][sb.sink].cand[0].parent)
                    {
                        vector<int> path;
                        path.push_back(sb.sink);
                        path.push_back(first_parent);
                        path_queue.push(path);
                    }

                    while (path_queue.size() != 0)
                    {
                        vector<int> cur_path = path_queue.front();
                        path_queue.pop();

                        if (cur_path.size() == min_hops)
                        {
                            cur_path.push_back(sc);
                            cand_path.push_back(cur_path);
                            continue;
                        }

                        for (const auto &parent : path_table_ver2[sc][cur_path.back()].cand[0].parent)
                        {
                            auto temp_path = cur_path;
                            temp_path.push_back(parent);
                            if (temp_path.size() == min_hops)
                            {
                                temp_path.push_back(sc);
                                cand_path.push_back(temp_path);
                                continue;
                            }
                            path_queue.push(temp_path);
                        }
                    }

                    //try all candidate path and routed best one
                    int best_index;
                    double best_cost = INT_MAX;
                    for (size_t i = 0; i < cand_path.size(); i++)
                    {
                        double try_cost = 0.0;
                        try_cost = compute_cost_for_gr2(n, cand_path[i], sb, routed_net);

                        if (try_cost < best_cost)
                        {
                            best_index = i;
                            best_cost = try_cost;
                        }
                    }
                    allpaths.push_back(cand_path[best_index]); //route best one
                    for (size_t j = 0; j < cand_path[best_index].size() - 1; j++)
                    {
                        if (edge_lut.count(make_pair(cand_path[best_index][j + 1], cand_path[best_index][j])) == 0)
                        {
                            edge_lut[make_pair(cand_path[best_index][j + 1], cand_path[best_index][j])] = 1;
                            add_channel_demand(cand_path[best_index][j + 1], cand_path[best_index][j]);
                        }

                        sources.push_back(cand_path[best_index][j]);
                    }
                }
                else //要比較不同hop的cost
                {
                    vector<vector<int>> cand_path;
                    const int &sc = sb.source;
                    for (size_t i = 0; i <= hop_limit; i++)
                    {
                        const auto &cur_cand = path_table_ver2[sb.source][sb.sink].cand[i];
                        int hop = cur_cand.hops;

                        //hop = 1 直接做
                        if (hop == 1)
                        {
                            vector<int> path;
                            path.push_back(sb.sink);
                            path.push_back(sc);
                            cand_path.push_back(path);
                            continue;
                        }

                        //hop > 1
                        queue<vector<int>> path_queue;
                        for (const auto &first_parent : cur_cand.parent)
                        {
                            vector<int> path;
                            path.push_back(sb.sink);
                            path.push_back(first_parent);
                            path_queue.push(path);
                        }

                        while (!path_queue.empty())
                        {
                            auto cur_path = path_queue.front();
                            path_queue.pop();

                            if (hop == cur_path.size())
                            {
                                cur_path.push_back(sc);
                                cand_path.push_back(cur_path);
                                continue;
                            }

                            //find index
                            int index, count = 0;
                            int cur_hop = cur_path.size() - 1;
                            int search_hop = hop - cur_hop;
                            for (const auto &cand : path_table_ver2[sc][cur_path.back()].cand)
                            {
                                if (cand.hops == search_hop)
                                {
                                    index = count;
                                    break;
                                }
                                count++;
                            }

                            for (const auto &parent : path_table_ver2[sc][cur_path.back()].cand[index].parent)
                            {
                                auto temp_path = cur_path;
                                temp_path.push_back(parent);
                                if (temp_path.size() == hop)
                                {
                                    temp_path.push_back(sc);
                                    cand_path.push_back(temp_path);
                                    continue;
                                }
                                path_queue.push(temp_path);
                            }
                        }
                    }

                    //print all candidate path
                    /*cout << "all candidate path : \n";
                    for (const auto &path : cand_path)
                    {
                        for (const auto &p : path)
                        {
                            cout << p << " ";
                        }
                        cout << endl;
                    }*/

                    //try all candidate path and route best one
                    double min_cost = INT_MAX;
                    int index, count = 0;
                    for (const auto &path : cand_path)
                    {
                        double temp = compute_cost_for_gr2(n, path, sb, routed_net);
                        if (temp < min_cost)
                        {
                            min_cost = temp;
                            index = count;
                        }
                        count++;
                    }
                    allpaths.push_back(cand_path[index]);
                    for (size_t i = 0; i < cand_path[index].size() - 1; i++)
                    {
                        if (edge_lut.count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
                        {
                            edge_lut[make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                            add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);
                        }
                        sources.push_back(cand_path[index][i]);
                    }
                }
                //continue;
            }

            bool routed = false;
            for (const auto &s : sources)
            {
                if (sb.sink == s)
                {
                    routed = true;
                    //cout << "   routed!" << endl;
                    break;
                }
            }

            if (routed)
                continue;

            vector<int> src_candidate;     //all sources to sink candidate with minimum fhops
            vector<vector<int>> cand_path; //save all candidate path
            int min_hops = INT_MAX;
            //find minimum f_hops
            //cout << "find minimum f_hops...";
            for (const auto &s : sources)
            {
                int min = INT_MAX;
                for (const auto &cand : path_table_ver2[s][sb.sink].cand)
                {
                    if (cand.hops < min)
                    {
                        min = cand.hops;
                    }
                }
                const int &tmp = min;
                if (tmp < min_hops && tmp != 0)
                    min_hops = tmp;
            }

            //save all source candidate with min hops
            //cout << "add source to src_candidate : ";
            for (const auto &s : sources)
            {
                int min = path_table_ver2[s][sb.sink].cand[0].hops;
                if (min == min_hops)
                {
                    //cout << s << " ";
                    src_candidate.push_back(s);
                }
            }
            //cout << endl;

            for (const auto &sc : src_candidate)
            {
                //cout << "search source " << sc << "'s all solution..." << endl;
                int cand_size = path_table_ver2[sc][sb.sink].cand.size();
                int constraint = (hop_limit + 1 > cand_size) ? cand_size - 1 : hop_limit;
                for (size_t i = 0; i <= constraint; i++)
                {
                    const auto &cur_cand = path_table_ver2[sc][sb.sink].cand[i];
                    int hop = cur_cand.hops;

                    //hop = 1 直接做
                    if (hop == 1)
                    {
                        vector<int> path;
                        path.push_back(sb.sink);
                        path.push_back(sc);
                        cand_path.push_back(path);
                        continue;
                    }

                    //hop > 1
                    queue<vector<int>> path_queue;
                    for (const auto &first_parent : cur_cand.parent)
                    {
                        vector<int> path;
                        path.push_back(sb.sink);
                        path.push_back(first_parent);
                        path_queue.push(path);
                    }

                    while (!path_queue.empty())
                    {
                        auto cur_path = path_queue.front();
                        path_queue.pop();

                        if (hop == cur_path.size())
                        {
                            cur_path.push_back(sc);
                            cand_path.push_back(cur_path);
                            continue;
                        }

                        //find index
                        int index, count = 0;
                        int cur_hop = cur_path.size() - 1;
                        int search_hop = hop - cur_hop;
                        for (const auto &cand : path_table_ver2[sc][cur_path.back()].cand)
                        {
                            if (cand.hops == search_hop)
                            {
                                index = count;
                                break;
                            }
                            count++;
                        }

                        for (const auto &parent : path_table_ver2[sc][cur_path.back()].cand[index].parent)
                        {
                            auto temp_path = cur_path;
                            temp_path.push_back(parent);
                            if (temp_path.size() == hop)
                            {
                                temp_path.push_back(sc);
                                cand_path.push_back(temp_path);
                                continue;
                            }
                            path_queue.push(temp_path);
                        }
                    }
                }
            }

            //try all candidate path and route best one
            double min_cost = INT_MAX;
            int index, count = 0;
            for (const auto &path : cand_path)
            {
                double temp = compute_cost_for_gr2(n, path, sb, routed_net);
                if (temp < min_cost)
                {
                    min_cost = temp;
                    index = count;
                }
                count++;
            }

            allpaths.push_back(cand_path[index]);
            for (size_t i = 0; i < cand_path[index].size() - 1; i++)
            {
                if (edge_lut.count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
                {
                    edge_lut[make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                    add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);
                }

                sources.push_back(cand_path[index][i]);
            }
        }

        //print subnet path
        /*for (const auto &ap : allpaths)
        {
            for (const auto &p : ap)
            {
                cout << p << " ";
            }
            cout << endl;
        }*/

        routing_tree(n, allpaths);
        routed_net++;
    }

    //distribute_channel_capacity();

    //show routing tree
    /*for (const auto &n : net)
    {
        cout << n.name << " routing tree : " << endl;
        show_tree(n.rtree_root);
        cout << endl;
    }*/
}

void FPGA_Gr::global_routing_ver3()
{
    subnetbased = true;
    int hop_limit = LIMIT_HOP; //最多嘗試與最小hop數差幾個hop的限制條件
    vector<pair<SubNet, int>> subnet_order;
    vector<vector<int>> *allpaths; //紀錄每個net的所有subpath
    map<pair<int, int>, int> *edge_lut;
    //vector<int> *sources;
    map<int, int> *sources;

    allpaths = new vector<vector<int>>[net.size()];
    edge_lut = new map<pair<int, int>, int>[net.size()];
    //sources = new vector<int>[net.size()];
    sources = new map<int, int>[net.size()];

    //allpaths.resize(net.size());
    //edge_lut.resize(net.size());
    //sources.resize(net.size());

    //算出subnet weight決定routing order
    auto t1 = clock();
    for (auto &n : net)
    {
        for (auto &sb : n.sbnet)
        {
            pair<SubNet, int> temp;
            const int &src = sb.source;
            const int &sink = sb.sink;
            const int &hops = path_table_ver2[src][sink].cand[0].hops;
            int weight = sb.weight;
            temp = make_pair(sb, weight);
            subnet_order.push_back(temp);
        }
    }
    sort(subnet_order.begin(), subnet_order.end(), comp_sbnetcost);
    //cout << "sort time = " << fixed << setprecision(2) << (double)(clock() - t1) / (double)CLOCKS_PER_SEC << " seconds\n";

    double total_time = 0.0;

    //start to route subnet
    int sub_order = 0;
    for (auto &sb : subnet_order)
    {
        auto t2 = clock();
        const auto &par_net_id = sb.first.parent_net;
        int source = sb.first.source;
        int sink = sb.first.sink;

        net[par_net_id].total_order += sub_order;
        sub_order++;

        //check 這個 sink 是不是被 route 過
        if (sources[par_net_id].count(sink) > 0)
        {
            continue;
        }
        //bool flag = false;
        /*for (auto &src : sources[par_net_id])
        {
            if (src == sink)
            {
                flag = true;
                break;
            }
        }
        if (flag)
        {
            continue;
        }*/

        /*if (par_net_id == 16100)
            cout << "current subnet (src, sink) = (" << source << ", " << sink << ")\n";*/

        //cout << "current subnet (src, sink) = (" << source << ", " << sink << ")\n";

        //start to routing
        vector<vector<int>> cand_path;
        vector<int> src_candidate;
        int cur_node = source;
        queue<pair<vector<int>, int>> path_queue; //(path, 剩餘hop)
        int minimum_hop;

        if (sources[par_net_id].size() == 0)
        {
            sources[par_net_id][source] = 1;
            src_candidate.push_back(source);
            minimum_hop = path_table_ver2[source][sink].cand[0].hops;
        }
        else
        {
            int min_hops = INT_MAX;
            for (const auto &s : sources[par_net_id])
            {
                int min = INT_MAX;
                for (const auto &cand : path_table_ver2[s.first][sink].cand)
                {
                    if (cand.hops < min)
                    {
                        min = cand.hops;
                    }
                }
                const int &tmp = min;
                if (tmp < min_hops && tmp != 0)
                    min_hops = tmp;
            }

            for (const auto &s : sources[par_net_id])
            {
                int min = path_table_ver2[s.first][sink].cand[0].hops;
                if (min == min_hops)
                {
                    src_candidate.push_back(s.first);
                }
            }

            minimum_hop = min_hops;
        }

        for (const auto &src : src_candidate)
        {
            /*if (par_net_id == 16100)
            {
                cout << "src cand : " << src << endl;
            }*/

            const auto &pt_init = path_table_ver2[src][sink];
            for (const auto &cand : pt_init.cand)
            {
                int hops = cand.hops;

                if (hops > minimum_hop + hop_limit)
                {
                    //cout << hops << endl;
                    continue;
                }

                //cout << "cand hop = " << hops << endl;

                /*if (par_net_id == 16100)
                    cout << "cand hop = " << hops << endl;*/

                for (const auto &par : cand.parent)
                {
                    vector<int> path;
                    path.push_back(sink);
                    path.push_back(par);
                    auto pq = make_pair(path, hops - 1);
                    path_queue.push(pq);

                    //cout << "construct path\n";
                    //cout << "push sink " << sink << endl;
                    //cout << "push node " << par << endl;

                    /*if (par_net_id == 16100)
                    {
                        cout << "construct path\n";
                        cout << "push sink " << sink << endl;
                        cout << "push node " << par << endl;
                    }*/
                }
            }
            //path_queue.push(path);

            while (!path_queue.empty())
            {
                auto cur_path = path_queue.front();
                path_queue.pop();
                //cout << "pop path\n";

                if (cur_path.first.back() == src) //done !
                {
                    cand_path.push_back(cur_path.first);
                    continue;
                }

                auto pt = path_table_ver2[src][cur_path.first.back()];

                for (auto &cand : pt.cand)
                {
                    if (cand.hops == 1 && cand.hops == cur_path.second)
                    {
                        auto temp_path = cur_path.first;
                        temp_path.push_back(src);
                        cand_path.push_back(temp_path);
                        //cout << "push source " << src << endl;
                    }
                    else if (cand.hops == cur_path.second)
                    {
                        for (auto &par : cand.parent)
                        {
                            auto temp_path = cur_path.first;
                            //cout << "push parent " << par << endl;
                            temp_path.push_back(par);
                            auto pq = make_pair(temp_path, cur_path.second - 1);
                            path_queue.push(pq);
                        }
                    }
                }
            }
        }

        /*for (auto &path : cand_path)
        {
            for (auto &p : path)
            {
                cout << p << " ";
            }
            cout << endl;
        }
        cout << endl;*/

        //try all candidate paths and route the best one
        double best = INT_MAX;
        int index, count = 0;

        /*if (par_net_id == 16100)
        {

            cout << "\n-----------------------------" << endl;
            if (par_net_id == 16100)
            {
                cout << "all srcs = ";
                for (auto &src : sources[par_net_id])
                {
                    cout << src.first << " ";
                }
                cout << endl;
            }
            cout << "cur sink = " << sb.first.sink << endl;
            cout << "all candidates : \n";
        }*/

        for (auto &path : cand_path)
        {
            int sink_num;

            //檢查path是否提前連到tree上了導致dummy node
            int check_idx = 1;
            for (int i = 1; i < path.size(); i++)
            {
                if (sources[par_net_id].count(path[i]) > 0)
                {
                    check_idx = i; //第一個連到tree的點
                    break;
                }
            }
            //pop多餘的點
            if (check_idx < path.size() - 1)
            {
                for (int i = 0; i < path.size() - 1 - check_idx; i++)
                {
                    path.pop_back();
                }
            }

            double cost = compute_cost_for_gr2(net[par_net_id], path, sb.first, sink_num);

            /*if (par_net_id == 16100)
            {
                cout << count << " : ";
                for (auto &p : path)
                {
                    cout << p << " ";
                }
                cout << ", sink num = " << sink_num;
                cout << ", cost = " << cost << endl;
            }*/

            if (cost < best)
            {
                best = cost;
                index = count;
            }

            count++;
        }

        //getchar();

        allpaths[par_net_id].push_back(cand_path[index]);

        /*if (par_net_id == 16100)
        {
            cout << "choose no." << index << endl;
        }*/

        for (size_t i = 0; i < cand_path[index].size() - 1; i++)
        {
            if (edge_lut[par_net_id].count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
            {
                edge_lut[par_net_id][make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);
            }
            //sources[par_net_id].push_back(cand_path[index][i]);
            sources[par_net_id][cand_path[index][i]] = 1;

            /*if (par_net_id == 16100)
                cout << "add " << cand_path[index][i] << " into sources\n";*/
        }
        //cout << "par = " << par_net_id << ", subnet : " << source << " " << sink << endl;

        total_time += ((double)(clock() - t2) / (double)CLOCKS_PER_SEC);

        routing_subtree(net[par_net_id], cand_path[index]);                        //add path to routing tree
        net[par_net_id].allpaths.push_back(make_pair(cand_path[index], sb.first)); //for rip-up and reroute

        /*if (par_net_id == 16100)
            getchar();*/
    }

    //cout << "route time = " << fixed << setprecision(2) << total_time << " seconds\n";

    //print allpaths
    /*for (const auto &npath : allpaths)
    {
        for (const auto &path : npath)
        {
            for (const auto &p : path)
                cout << p << " ";
            
            cout << endl;
        }
    }*/

    //construct routing tree
    /*for (size_t i = 0; i < net.size(); i++)
    {
        routing_tree(net[i], allpaths[i]);
    }*/

    /*cout << "16100 all paths" << endl;

    for (const auto &path : allpaths[16100])
    {
        for (const auto &p : path)
            cout << p << " ";

        cout << endl;
    }*/

    delete[] allpaths;
    delete[] edge_lut;
    delete[] sources;
}

double FPGA_Gr::compute_cost_for_gr2(Net &n, const vector<int> &path, const SubNet &sbnet, int &sink_num)
{
    double cost = 0.0, cost_path = 0.0, appr_tdm = 0.0;
    double weight = sbnet.weight;
    double hop_num = path.size() - 1;
    double total_used = 0.0, net_cost = 0.0;
    double total_his_cost = 0.0;
    double total_weight = 0.0;
    double cost_par = 0.0;
    sink_num = 1;

    map<int, int> sink_lut;
    for (const auto &sk : n.sink)
    {
        sink_lut[sk.id] = sk.weight;
    }

    for (size_t i = 0; i < path.size() - 1; i++)
    {
        int cap = channel_capacity[make_pair(path[i + 1], path[i])];

        cap = (cap == 0) ? 1 : cap;

        //double sig_weight = n.edge_crit[make_pair(path[i + 1], path[i])];
        const int &direct = (path[i + 1] < path[i]) ? 0 : 1; //min-->max : 0, max-->min : 1
        double ch_used = channel_used(path[i + 1], path[i]);

        double before_tdm = (double)(ch_used) / (double)cap;
        double appr_tdm = (double)(ch_used + 1) / (double)cap; //src to sink appr. tdm

        auto ch_name = get_channel_name(path[i], path[i + 1]);
        auto ch = map_to_channel[ch_name];
        double his_cost = ch->history_used[direct];

        //check weight

        if (i > 0)
        {
            //check if current node is a sink ?

            if (sink_lut.count(path[i]) > 0)
            {
                sink_num++;
                /*if (n.id == 5730)
                    cout <<  path[i] << " is a sink" << endl;*/
                //check if sink weight is larger than current weight
                if (sink_lut[path[i]] > weight)
                {
                    weight = sink_lut[path[i]];
                }
            }
        }

        total_weight += weight;
        int wirelength = i + 1;

        if (ceil(appr_tdm) <= ceil(before_tdm))
        {
            ch_used = 0;
        }

        appr_tdm = (appr_tdm < 1) ? 1 : appr_tdm;
        double alpha = his_cost / (double)round;
        double congestion_cost = cost_par + (1 + alpha) * (weight + 6.5 * appr_tdm);
        double cost_cur = congestion_cost;

        cost_par = cost_cur;
        cost_path += cost_cur;
        total_his_cost += his_cost / (double)round;
    }

    if (sink_lut.count(path.back()) > 0)
        sink_num++;

    cost = cost_path; //history + current path cost
    //cost /= (double)sink_num;

    return cost;
}

void FPGA_Gr::routing_tree(Net &n, const vector<vector<int>> &subpath)
{
    Tree_Node *root = new Tree_Node();
    root->parent = NULL;
    root->fpga_id = subpath[0].back(); //source
    n.rtree_root = root;

    for (const auto &path : subpath)
    {
        const int &par_id = path.back();
        //cout << "parent is fpga " << par_id << endl;
        Tree_Node *parent = search_node(n.rtree_root, par_id);

        //cout << "find parent fpga " << parent->fpga_id << endl;

        for (int i = path.size() - 2; i >= 0; i--)
        {
            Tree_Node *new_node = new Tree_Node();
            new_node->fpga_id = path[i];
            new_node->parent = parent;
            parent->children.push_back(new_node);
            //cout << "add child fpga " << new_node->fpga_id << endl;
            parent = new_node;
        }
    }

    compute_edge_weight(n, n.rtree_root);
}

void FPGA_Gr::routing_subtree(Net &n, const vector<int> &subpath)
{
    if (n.rtree_root == NULL || n.rtree_root == nullptr)
    {
        Tree_Node *root = new Tree_Node();
        root->parent = NULL;
        root->fpga_id = subpath.back();
        n.rtree_root = root;
    }

    const int &par_id = subpath.back();
    //cout << "parent is fpga " << par_id << endl;
    Tree_Node *parent = search_node(n.rtree_root, par_id);

    //cout << "find parent fpga " << parent->fpga_id << endl;

    for (int i = subpath.size() - 2; i >= 0; i--)
    {
        Tree_Node *new_node = new Tree_Node();
        new_node->fpga_id = subpath[i];
        new_node->parent = parent;
        parent->children.push_back(new_node);
        parent = new_node;
    }
}

void FPGA_Gr::record_net_channel_used()
{
    //record that the net use which channels
    for (auto &n : net)
    {
        Net *n_ptr = &n;
        queue<Tree_Node *> fifo_queue;
        fifo_queue.push(n.rtree_root);

        while (fifo_queue.size() != 0)
        {
            Tree_Node *cur = fifo_queue.front();
            fifo_queue.pop();

            if (cur->children.size() == 0)
                continue;

            for (const auto &child : cur->children)
            {
                //紀錄channel內包含的net及對應edge weight (old)
                auto ch_name = get_channel_name(cur->fpga_id, child->fpga_id);
                Channel *chan = map_to_channel[ch_name];
                pair<Net *, double> node;
                node.first = n_ptr;
                node.second = child->edge_weight; // 2/29 update !
                chan->net_ch_weight.push_back(node);

                //紀錄channel內包含的net及對應edge weight (new)
                //add_net_to_channel(cur->fpga_id, child->fpga_id, n.id, child->edge_weight);

                //紀錄net使用了哪些channels
                n.channels.push_back(ch_name);

                fifo_queue.push(child);
            }
        }
    }

    //show_net_channel_table();
    cout << "OK" << endl;
}

void FPGA_Gr::show_net_channel_table()
{
    for (const auto &ch : map_to_channel)
    {
        /*
        if (channel_demand[ch.first] == 0)
        {
            continue;
        }

        cout << "Channel (" << ch.first.first << ", " << ch.first.second << ") : " << endl;
        cout << "Total used record in channel table : " << ch.second->net_ch_weight.size() << endl;
        cout << "Total used record in channel_demand : " << channel_demand[ch.first] << endl;
        cout << "Check all channel have been recorded in net...";
        */
        bool check = true;
        for (const auto &n : ch.second->net_ch_weight)
        {
            bool find = false;
            for (const auto &chan : n.first->channels) //search all channels in the net
            {
                if (chan == ch.first)
                {
                    find = true;
                }
            }
            if (!find)
                check = false;
        }
        cout << ((check) ? "OK" : "Error") << endl;
        cout << endl;
    }
}

void FPGA_Gr::compute_edge_weight(Net &n, Tree_Node *root)
{
    //set all sink weight
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        for (const auto &sink : n.sink)
        {
            if (sink.id == cur->fpga_id)
            {
                cur->sink_weight = sink.weight;
                break;
            }
        }

        for (auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }

    //set max value and edge weight
    fifo_queue.push(root);
    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        int max = max_weight(cur);
        if (max > cur->sink_weight)
        {
            cur->max_value = max;
            cur->edge_weight = max;
        }
        else
        {
            cur->max_value = cur->sink_weight;
            cur->edge_weight = cur->sink_weight;
        }

        for (auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }
}

int FPGA_Gr::channel_used(int s, int t) //return channel(direct s-->t) used
{
    const int &demand = channel_demand[make_pair(s, t)];
    return demand;
}

double FPGA_Gr::channel_TDM(int s, int t) //return src to target appr. TDM
{
    const int &demand = channel_demand[make_pair(s, t)];
    double TDM = ceil((double)demand / (double)channel_capacity[make_pair(s, t)]);
    //TDM = (TDM <= 1) ? 1 : (int)ceil(TDM / 8) * 8;
    return TDM;
}

double FPGA_Gr::compute_TDM_cost()
{
    double cost = 0.0;
    double total_tdm_ratio = 0.0;
    double total_tree_edge = 0.0;

    for (auto &n : net)
    {
        n.net_initialize();

        if (subnetbased)
        {
            compute_edge_weight(n, n.rtree_root);
        }
        //cout << n.name << endl;

        n.cost = 0.0;
        queue<Tree_Node *> fifo_queue;
        for (const auto &child : n.rtree_root->children)
        {
            fifo_queue.push(child);
            //n.edge_crit[make_pair(n.rtree_root->fpga_id, child->fpga_id)]++;
            n.total_tree_edge++;

            //record signal pass channel
            auto ch_name = get_channel_name(n.rtree_root->fpga_id, child->fpga_id);
            auto ch = map_to_channel[ch_name];
            int idx = (n.rtree_root->fpga_id > child->fpga_id) ? 1 : 0;
            ch->passed_nets[idx].push_back(&n);
        }

        while (fifo_queue.size() != 0)
        {

            Tree_Node *cur = fifo_queue.front();
            fifo_queue.pop();

            const int &par_id = cur->parent->fpga_id;
            const int &cur_id = cur->fpga_id;
            //cout << "par cur = " << par_id << " " << cur_id << endl;
            double tdm_ratio = channel_TDM(par_id, cur_id);

            n.max_tdm = (tdm_ratio > n.max_tdm) ? tdm_ratio : n.max_tdm;
            n.min_tdm = (tdm_ratio < n.min_tdm) ? tdm_ratio : n.min_tdm;

            n.total_tdm += tdm_ratio;
            n.cost += (tdm_ratio * (double)cur->edge_weight);
            n.total_edge_weight += (double)cur->edge_weight;
            total_tdm_ratio += tdm_ratio;

            //紀錄net中channel資訊
            auto ch_name = get_channel_name(par_id, cur_id);
            Channel *chan = map_to_channel[ch_name];

            double repeat_ch = 1.0;

            if (RRtimes.count(ch_name) > 0)
            {
                double times = RRtimes[ch_name];
                repeat_ch -= (0.3 * times);
            }
                
            congestion_map[ch_name] += tdm_ratio * repeat_ch * (double)cur->edge_weight;
            maxtdm = (tdm_ratio > maxtdm) ? tdm_ratio : maxtdm;
            mintdm = (tdm_ratio < mintdm) ? tdm_ratio : mintdm;

            for (auto &child : cur->children)
            {
                fifo_queue.push(child);
                //n.edge_crit[make_pair(cur->fpga_id, child->fpga_id)]++;
                n.total_tree_edge++;

                //record signal pass channel
                auto ch_name = get_channel_name(cur->fpga_id, child->fpga_id);
                auto ch = map_to_channel[ch_name];
                int idx = (cur->fpga_id > child->fpga_id) ? 1 : 0;
                ch->passed_nets[idx].push_back(&n);
            }
        }

        cost += n.cost;
        total_tree_edge += n.total_tree_edge;
        n.signal_weight = n.cost / (double)n.sink.size();
        maxsgw = (n.signal_weight > maxsgw) ? n.signal_weight : maxsgw;
        minsgw = (n.signal_weight < minsgw) ? n.signal_weight : minsgw;
        //cout << n.name << " => done !" << endl;
    }

    /*-----------
    total_tdm_ratio = 0.0;
    for (const auto &chd:channel_demand)
    {
        total_tdm_ratio += channel_TDM(chd.first.first, chd.first.second);
    }
    avg_tdm_ratio = total_tdm_ratio / channel_demand.size();
    ------------*/

    old_map_vec.clear();
    total_cost = cost;
    return cost;
}

double FPGA_Gr::comptue_tree_TDM_cost(Tree_Node *root)
{
    double cost = 0.0;

    queue<Tree_Node *> fifo_queue;
    for (const auto &child : root->children)
    {
        fifo_queue.push(child);
    }

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        const int &par_id = cur->parent->fpga_id;
        const int &cur_id = cur->fpga_id;
        double tdm_ratio = channel_TDM(par_id, cur_id);
        cost += (tdm_ratio * (double)cur->edge_weight);

        for (auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }

    return cost;
}

bool FPGA_Gr::find_rip_up_edge(Net &n, int &parent, int &child)
{
    //find the node that max value is lager than its sink weight
    queue<Tree_Node *> fifo_queue;
    for (const auto &child : n.rtree_root->children)
    {
        fifo_queue.push(child);
    }

    bool find = false;

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->max_value > cur->sink_weight && cur->sink_weight != 0)
        {
            parent = cur->fpga_id;
            for (const auto &c : cur->children)
            {
                if (c->edge_weight == cur->max_value)
                {
                    find = true;
                    child = c->fpga_id;
                    break;
                }
            }
            break;
        }

        for (auto &child : cur->children)
        {
            fifo_queue.push(child);
        }
    }

    return find;
}

Tree_Node *FPGA_Gr::rip_up_edge(Net &n, const int &par, const int &chi)
{
    sub_channel_demand(par, chi);
    Tree_Node *child = search_node(n.rtree_root, chi);
    Tree_Node *parent = child->parent;

    //delete parent's child
    for (auto it = parent->children.begin(); it != parent->children.end(); ++it)
    {
        if ((*it)->fpga_id == chi)
        {
            parent->children.erase(it);
            break;
        }
    }

    //delete child's parent
    child->parent = NULL;

    //delete used channel
    int s = min(par, chi);
    int t = max(par, chi);
    Channel *ch = map_to_channel[make_pair(s, t)];

    //delete this net from channel (old)
    for (auto it = ch->net_ch_weight.begin(); it != ch->net_ch_weight.end(); ++it)
    {
        if (it->first->name == n.name)
        {
            ch->net_ch_weight.erase(it);
            break;
        }
    }

    //delete this net from channel (3/2 new)
    //delete_net_from_channel(s, t, n.id);

    //delete this channel from net
    for (auto it = n.channels.begin(); it != n.channels.end(); ++it)
    {
        if (*it == ch->name)
        {
            n.channels.erase(it);
            break;
        }
    }

    return child;
}

void FPGA_Gr::reroute_edge(Net &n, const int &par, Tree_Node *child, double old_cost)
{
    int old = 0;

    vector<Tree_Node *> node_list; //save candidate reconnect node
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(n.rtree_root);

    //find target nodes and save them to the node list
    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->max_value > child->max_value)
        {
            node_list.push_back(cur);
        }

        for (auto &chi : cur->children)
        {
            fifo_queue.push(chi);
        }
    }

    if (node_list.size() == 0) //cannot find node to reconnect => 恢復
    {
        Tree_Node *parent = search_node(n.rtree_root, par);
        child->parent = parent;
        parent->children.push_back(child);
        add_channel_demand(parent->fpga_id, child->fpga_id);

        //將net加回去channel的net list中 (old)
        const auto ch_name = get_channel_name(parent->fpga_id, child->fpga_id);
        Channel *target_ch = map_to_channel[ch_name];
        pair<Net *, double> node;
        node.first = &n;
        node.second = child->edge_weight;
        target_ch->net_ch_weight.push_back(node);

        //將net加回去channel的net list中 (3/2 new)
        //add_net_to_channel(parent->fpga_id, child->fpga_id, n.id, child->edge_weight);

        //將channel加回去net中
        n.channels.push_back(ch_name);

        return;
    }

    //compute current cost
    compute_edge_weight(n, n.rtree_root);
    compute_edge_weight(n, child);

    double cur_cost = old_cost - n.cost + comptue_tree_TDM_cost(n.rtree_root) + comptue_tree_TDM_cost(child);

    //reroute
    vector<int> cand_path;
    queue<pair<vector<int>, double>> path_queue; //record path and cost
    double max_weight = child->max_value;
    double best_cost = old_cost;
    Tree_Node *target;

    /***************using optimal maze routing to reconnect*************/
    bool done = false;
    int start = child->fpga_id;
    //cout << "Start fpge id = " << start << endl;
    //int end = node->fpga_id;
    int count = 0;
    vector<int> visit;
    visit.assign(fpga_num, 0);

    //cout << "設定不可拜訪的點..." << endl;
    visit[child->fpga_id] = 1; //自己已拜訪

    //subtree的點不能被拜訪，設為1
    fifo_queue.push(child);
    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();
        if (visit[cur->fpga_id] == 0)
        {
            visit[cur->fpga_id] = 1;
            count++;
        }

        for (auto &chi : cur->children)
        {
            fifo_queue.push(chi);
        }
    }

    //cout << "從起點往外擴一步..." << endl;
    //start to reconnect (先走一步)
    for (const auto &nbr : fpga[start].nbr_pair)
    {
        if (visit[nbr.first] == 1)
        {
            //cout << "已拜訪過 " << nbr.first << endl;
            continue;
        }

        vector<int> temp;      //用來存走過的路徑
        temp.push_back(start); //起點
        temp.push_back(nbr.first);

        //針對有路過這個channel的所有net都要增加cost
        //cout << "針對有路過這個channel的所有net都要增加cost..." << endl;
        auto ch_name = get_channel_name(start, nbr.first);
        Channel *target_ch = map_to_channel[ch_name];
        int ch_cap = target_ch->capacity;

        double tdm_ratio = (double)(channel_used(start, nbr.first) + 1) / (double)ch_cap;

        //計算受影響net的cost增加量總和
        double increased_value = 0.0;
        double old_tdm = channel_TDM(start, nbr.first);
        for (auto &inf_net : target_ch->net_ch_weight)
        {
            //cout << "new tdm ratio = " << tdm_ratio << ", old tdm ratio = " << old_tdm << ", edge weight = " << setw(2) << inf_net.second << "  : ";
            //cout << "increase " << (tdm_ratio - old_tdm) * inf_net.second << endl;
            increased_value += (tdm_ratio - old_tdm) * inf_net.second;
        }
        //cout << "受影響net的cost增加量...increase " << increased_value << endl;

        //計算當前net的cost增加量
        //cout << "計算當前net的cost增加量...";
        double cur_increased_value = max_weight * tdm_ratio;
        //cout << "increase " << cur_increased_value << endl;

        path_queue.push(make_pair(temp, cur_increased_value + increased_value));
        visit[nbr.first] = 1;
        count++;

        //print path
        //cout << "Push path : ";
        /*for (const auto &p : temp)
        {
            cout << p << " ";
        }*/
        //cout << "\n\n";
    }

    //getchar();
    //cout << endl;

    while (count != fpga_num && !path_queue.empty())
    {
        pair<vector<int>, double> path = path_queue.front();
        path_queue.pop();

        //cout << "Pop path : ";
        /*for (const auto &p : path.first)
        {
            cout << p << " ";
        }*/
        //cout << endl;

        if (cur_cost + path.second > old_cost)
        {
            //cout << "cost大於old cost...捨棄" << endl;
            continue;
        }

        //檢查是否已經連到終點
        //cout << "檢查是否已經連到終點..." << endl;
        for (const auto &node : node_list)
        {
            if (path.first.back() == node->fpga_id)
            {
                //cout << "到達終點 !!" << endl;
                done = true;
                target = node;
                //getchar();
                break;
            }
        }

        if (done)
        {
            if (cur_cost + path.second < best_cost)
            {
                //cout << "new cost < old cost => update !" << endl;
                cand_path = path.first;
                best_cost = path.second + cur_cost;
            }
            break;
        }

        //還沒到目的地，繼續走下一步
        for (const auto &nbr : fpga[path.first.back()].nbr_pair)
        {
            if (visit[nbr.first] == 1)
            {
                continue;
            }

            pair<vector<int>, double> temp = path;
            int cur = temp.first.back();
            temp.first.push_back(nbr.first);
            int next = temp.first.back();

            auto ch_name = get_channel_name(cur, next);
            Channel *target_ch = map_to_channel[ch_name];
            int ch_cap = target_ch->capacity;

            double tdm_ratio = (double)(channel_used(cur, next) + 1) / (double)ch_cap;

            //計算受影響net的cost增加量總和
            //cout << "針對有路過這個channel的所有net都要增加cost..." << endl;
            double increased_value = 0.0;
            double old_tdm = channel_TDM(cur, next);
            for (auto &inf_net : target_ch->net_ch_weight)
            {
                increased_value += (tdm_ratio - old_tdm) * inf_net.second;
            }
            //cout << "受影響net的cost增加量...increase " << increased_value << endl;

            //計算當前net的cost增加量
            //cout << "計算當前net的cost增加量...";
            double cur_increased_value = max_weight * tdm_ratio;
            //cout << "increase " << cur_increased_value << endl;

            temp.second += increased_value + cur_increased_value;
            path_queue.push(temp);
            visit[nbr.first] = 1;
            count++;

            //print path
            //cout << "Push path : ";
            /*for (const auto &p : temp.first)
            {
                cout << p << " ";
            }*/
            //cout << "\n\n";
        }
    }

    if (cand_path.size() != 0)
    {
        //cout << "開始對path ";
        /*for (const auto &p : cand_path)
        {
            cout << p << " ";
        }*/
        //cout << "進行繞線\n\n";

        //compute_TDM_cost();
        total_cost = best_cost;
        //cout << "update total cost to " << total_cost << endl;

        //cout << "child fpga id = " << child->fpga_id << endl;
        Tree_Node *cur = child;
        //cout << "current fpga id = " << cur->fpga_id << endl;

        for (size_t i = 1; i < cand_path.size() - 1; i++)
        {
            Tree_Node *new_node = new Tree_Node();
            new_node->fpga_id = cand_path[i];
            cur->parent = new_node;
            new_node->children.push_back(cur);
            add_channel_demand(cur->fpga_id, cand_path[i]);
            //cout << cur->fpga_id << " " << cand_path[i] << endl;

            auto ch_name = get_channel_name(cur->fpga_id, cand_path[i]);
            Channel *target_ch = map_to_channel[ch_name];
            //cout << "route channel " << ch_name.first << " " << ch_name.second << endl;

            //新增net增加的channel
            n.channels.push_back(ch_name);

            //新增channel增加的net
            pair<Net *, double> node;
            node.first = &n;
            node.second = max_weight; //update weight
            target_ch->net_ch_weight.push_back(node);

            cur = new_node;
        }

        cur->parent = target;
        target->children.push_back(cur);
        add_channel_demand(cur->fpga_id, target->fpga_id);

        const auto &ch_name = get_channel_name(cur->fpga_id, target->fpga_id);
        Channel *target_ch = map_to_channel[ch_name];

        //新增net增加的channel
        n.channels.push_back(ch_name);

        //新增channel增加的net
        pair<Net *, double> node;
        node.first = &n;
        node.second = max_weight; //update weight
        target_ch->net_ch_weight.push_back(node);
    }
    else
    {
        Tree_Node *parent = search_node(n.rtree_root, par);
        child->parent = parent;
        parent->children.push_back(child);
        add_channel_demand(parent->fpga_id, child->fpga_id);

        const auto &ch_name = get_channel_name(parent->fpga_id, child->fpga_id);
        Channel *target_ch = map_to_channel[ch_name];

        //新增net增加的channel
        n.channels.push_back(ch_name);

        //新增channel增加的net
        pair<Net *, double> node;
        node.first = &n;
        node.second = max_weight; //update weight
        target_ch->net_ch_weight.push_back(node);
        total_cost = old_cost;
    }
}

void FPGA_Gr::rip_up_reroute(time_t t)
{
    //total_cost = compute_TDM_cost();
    cout << "initial cost = " << fixed << setprecision(0) << total_cost << endl;
    double init = total_cost;

    while (true)
    {
        int count = 1;
        bool update = false;

        for (auto &n : net)
        {
            Tree_Node *child;
            int par, chi;
            bool find = find_rip_up_edge(n, par, chi);

            if (chi == n.source || !find)
            {
                continue;
            }

            //cout << "rip-up edge..." << endl;
            child = rip_up_edge(n, par, chi);
            double old_cost = total_cost;
            reroute_edge(n, par, child, old_cost);
            double new_cost = total_cost;

            if (new_cost < old_cost)
            {
                update = true;
                cout << "Iter " << count << " : ";
                cout << "TDM cost = " << fixed << setprecision(0) << total_cost;
                cout << ", runtime = " << (double)(clock() - t) / (double)CLOCKS_PER_SEC << " seconds\n";
            }
        }

        count++;

        if (!update)
        {
            cout << "not update => stop !" << endl;
            cout << "improve " << fixed << setprecision(2) << (init - total_cost) / init << "%" << endl;
            break;
        }
    }
    //compute_TDM_cost();
}

vector<pair<int, int>> FPGA_Gr::sub_allchannels(Net &n, Tree_Node *sbtree_root, vector<SubNet> &allsubnets, map<int, int> &all_rip_nodes) //return all channels of tree and 把 channel的demand都-1 (RR用)
{
    vector<pair<int, int>> allchannels;
    map<int, int> sink_lut;

    if (sbtree_root == NULL)
    {
        cout << "[error] function(allchannels) root is null !" << endl;
        exit(1);
    }

    for (const auto &sk : n.sink) //把net中所有sink放入lut中
    {
        sink_lut[sk.id] = 1;
    }

    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(sbtree_root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();
        all_rip_nodes[cur->fpga_id] = 1;

        //check當前pop出來的點是不是sink
        if (sink_lut.count(cur->fpga_id) != 0)
        {
            SubNet temp;
            temp.source = n.source;
            temp.sink = cur->fpga_id;
            temp.parent_net = n.id;

            for (const auto &sk : n.sink)
            {
                if (cur->fpga_id == sk.id)
                {
                    temp.weight = sk.weight;
                }
            }

            allsubnets.push_back(temp);
        }

        if (cur->children.size() == 0)
            continue;

        for (const auto &child : cur->children)
        {
            pair<int, int> chan;
            chan = make_pair(cur->fpga_id, child->fpga_id);
            sub_channel_demand(chan.first, chan.second);
            allchannels.push_back(chan);
            fifo_queue.push(child);
        }
    }

    sort(allsubnets.begin(), allsubnets.end(), comp_weight);
    return allchannels;
}

map<int, int> ret_all_node_and_edges(Tree_Node *root, map<pair<int, int>, int> &edge_lut) //return all nodes in the tree
{
    map<int, int> allnodes;

    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(root);

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        //allnodes.push_back(cur->fpga_id);
        allnodes[cur->fpga_id] = 1;
        //cout << "push node " << cur->fpga_id << " as source." << endl;

        if (cur->children.size() == 0)
            continue;

        for (const auto &child : cur->children)
        {
            edge_lut[make_pair(cur->fpga_id, child->fpga_id)] = 1;
            fifo_queue.push(child);
        }
    }

    return allnodes;
}

void FPGA_Gr::max_subpath_RR() //RR
{
    double old_cost = total_cost;

    //決定要RR的Net order
    for (auto &n : net)
    {
        double net_avg_skw = n.total_sink_weight / (double)n.sink.size();
        //double edge_weight_penalty = n.total_edge_weight / (double)n.sink.size();
        double edge_weight_penalty = (n.total_edge_weight / (double)n.total_tree_edge) - net_avg_skw;
        //double weight_order = n.total_order / (double)n.sink.size(); //越先被route越先reroute

        n.criticality = net_avg_skw * n.total_tdm + edge_weight_penalty;
        //n.criticality *= weight_order;

        //n.criticality = net_avg_skw * n.total_tdm;
    }
    sort(net.begin(), net.end(), comp_netcrit);
    //random_shuffle(net.begin(), net.end());

    for (int i = 0; i < (int)net.size(); i++)
    {
        if (i > 1.0 / 2.0 * net.size()) //挑出criticality大小前面1/2的net來RR
            break;

        auto &n = net[i];

        n.cost = comptue_tree_TDM_cost(n.rtree_root);
        double new_cost = old_cost - n.cost;

        //find rip-up path
        double max_cost = 0;
        int sb_idx, count = 0;
        int sink_num;
        vector<pair<int, int>> inf_channel_sub;
        vector<pair<int, int>> inf_channel_add;

        for (auto &sb : n.allpaths)
        {
            int temp;
            double cost = compute_cost_for_gr2(n, sb.first, sb.second, temp);

            if (cost > max_cost)
            {
                max_cost = cost;
                sb_idx = count;
                sink_num = temp;
            }
            count++;

            /*if (n.id == 3236)
            {
                cout << "before all paths : ";
                for (auto &p : sb.first)
                {
                    cout << p << " ";
                }
                cout << endl;
            }*/
        }

        //rip-up idx=sb_idx subne
        auto &rip_path = n.allpaths[sb_idx].first;
        int rip_fpga_id;

        if (sink_num > 1) //find which node we want to rip up
        {
            for (int i = rip_path.size() - 1; i >= 0; i--)
            {
                int cur_id = rip_path[i];
                bool isSink = false;

                for (const auto &sk : n.sink)
                {
                    if (cur_id == sk.id)
                    {
                        isSink = true;
                        break;
                    }
                }

                if (isSink)
                {
                    rip_fpga_id = cur_id;
                    break;
                }
            }
        }
        else
        {
            rip_fpga_id = rip_path[0];
        }

        /*if (n.id == 3236)
        {
            cout << "before tree" << endl;
            show_tree(n.rtree_root);
        }*/

        Tree_Node *node = search_node(n.rtree_root, rip_fpga_id);
        Tree_Node *node_par = node->parent;

        //cout << "before" << endl;
        //show_tree(n.rtree_root);

        //找出最後要拔掉的點 (這個點以下node都被拔掉，包含這個點)
        Tree_Node *last_rip = node;
        while (node_par->children.size() - 1 == 0 && node_par->fpga_id != n.source)
        {
            //int cur_rip = node_par->fpga_id;
            last_rip = node_par;
            node_par = node_par->parent;
        }

        //cout << "delete node : " << last_rip->fpga_id << endl;
        /*if (n.id == 3236)
        {
            cout << "delete node : " << last_rip->fpga_id << endl;
        }*/

        //拔
        for (auto it = node_par->children.begin(); it != node_par->children.end(); ++it)
        {
            if ((*it)->fpga_id == last_rip->fpga_id)
            {
                node_par->children.erase(it);
                break;
            }
        }

        pair<int, int> last_chan = make_pair(last_rip->parent->fpga_id, last_rip->fpga_id);
        sub_channel_demand(last_chan.first, last_chan.second);
        inf_channel_sub.push_back(last_chan);
        last_rip->parent = NULL; //delete node parent

        //cout << "-----------------\n";
        //cout << "after" << endl;
        compute_edge_weight(n, n.rtree_root);
        //show_tree(n.rtree_root);

        /*if (n.id == 3236)
        {
            cout << "after" << endl;
            show_tree(n.rtree_root);
        }*/

        map<int, int> rip_node_lut;                                              //存放所有被拔掉的點
        vector<SubNet> allsubnets;                                               //要reroute的sink
        auto influence = sub_allchannels(n, last_rip, allsubnets, rip_node_lut); //把受影響channel的demand都-1，並記錄下來，以及找出要reroute的sink
        inf_channel_sub.insert(inf_channel_sub.end(), influence.begin(), influence.end());

        /*-------------------------------------更新net中allpaths的資訊-------------------------------------*/
        /*for (const auto &sb : allsubnets)
        {
            rip_node_lut[sb.sink] = 1;
        }*/

        vector<pair<vector<int>, SubNet>> new_allpaths;
        vector<vector<int>> allpaths; //記錄剩下的path+所有reroute完的path

        int i_checkpath = 0;
        vector<int> first_path_temp;
        for (const auto &path : n.allpaths)
        {
            bool flag = false; //檢查這個path的點是否有被拔掉

            ///
            vector<int> path_temp;
            for (auto &p : path.first)
            {
                if (rip_node_lut.count(p) == 0) //保留沒有被拔掉的點
                {
                    path_temp.push_back(p);
                }
                else
                {
                    flag = true;
                }
            }

            if (flag) //subpath中的sink還在
            {
                new_allpaths.push_back(path);
                allpaths.push_back(path.first);
            }
            else
            {
                SubNet temp;
                temp.source = n.source;
                temp.sink = path_temp.front();
                temp.parent_net = n.id;
                temp.weight = 0; //代表不是sink

                new_allpaths.push_back(make_pair(path_temp, temp));
                allpaths.push_back(path_temp);
            }
            ///
            /*
            for (auto &p : path.first)
            {
                if (rip_node_lut.count(p) == 0 && i_checkpath == 0)
                {
                    first_path_temp.push_back(p); //保留第一條沒有被拔掉的點
                }
                else if (rip_node_lut.count(p) != 0 && i_checkpath != 0)
                {
                    flag = true; //有被拔掉
                    break;
                }
            }

            if (i_checkpath == 0)
            {
                SubNet temp;
                temp.source = n.source;
                temp.sink = first_path_temp.front();
                temp.parent_net = n.id;
                temp.weight = 0; //代表不是sink

                new_allpaths.push_back(make_pair(first_path_temp, temp));
                allpaths.push_back(first_path_temp);
            }
            else if (!flag) //if沒被拔掉
            {
                new_allpaths.push_back(path);
                allpaths.push_back(path.first);
            }
            */
            i_checkpath++;
        }

        /*--------------------------------------backup old allpahts--------------------------------*/
        vector<vector<int>> old_allpaths;
        vector<pair<vector<int>, SubNet>> old_netallpaths;
        for (const auto &apath : n.allpaths)
        {
            old_netallpaths.push_back(apath);
            old_allpaths.push_back(apath.first);
        }
        n.allpaths = new_allpaths;
        /*-------------------------------------------------------------------------------------------*/

        //reroute all subnets
        map<pair<int, int>, int> edge_lut;
        map<int, int> sources = ret_all_node_and_edges(n.rtree_root, edge_lut); //rip up後的tree每個點都可當要reroute的sink的source
        int hop_limit = LIMIT_HOP;                                              //最多嘗試與最小hop數差幾個hop的限制條件

        for (auto &sb : allsubnets)
        {
            const auto &par_net_id = sb.parent_net;
            int source = sb.source;
            int sink = sb.sink;

            //check 這個 sink 是不是被 route 過
            if (sources.count(sink) > 0)
            {
                continue;
            }

            //cout << "current subnet (src, sink) = (" << source << ", " << sink << ")\n";

            //start to routing
            vector<vector<int>> cand_path;
            vector<int> src_candidate;
            int cur_node = source;
            queue<pair<vector<int>, int>> path_queue; //(path, 剩餘hop)
            int minimum_hop;

            int min_hops = INT_MAX;
            for (const auto &s : sources)
            {
                int min = INT_MAX;
                for (const auto &cand : path_table_ver2[s.first][sink].cand)
                {
                    if (cand.hops < min)
                    {
                        min = cand.hops;
                    }
                }
                const int &tmp = min;
                if (tmp < min_hops && tmp != 0)
                    min_hops = tmp;
                src_candidate.push_back(s.first); //20200628 add...(1)
            }

            //src_candidate = sources;      //20200628 delete...(1)
            minimum_hop = min_hops;

            //print all source candidate
            /*cout << "find source candidates : ";
            for (const auto &src : src_candidate)
            {
                cout << src << " ";
            }
            cout << endl;*/

            for (const auto &src : src_candidate)
            {
                const auto &pt_init = path_table_ver2[src][sink];
                for (const auto &cand : pt_init.cand)
                {
                    int hops = cand.hops;

                    if (hops > minimum_hop + hop_limit)
                    {
                        //cout << hops << endl;
                        continue;
                    }

                    //cout << "cand hop = " << hops << endl;
                    for (const auto &par : cand.parent)
                    {
                        vector<int> path;
                        path.push_back(sink);
                        path.push_back(par);
                        auto pq = make_pair(path, hops - 1);
                        path_queue.push(pq);

                        //cout << "construct path\n";
                        //cout << "push sink " << sink << endl;
                        //cout << "push node " << par << endl;
                    }
                }
                //path_queue.push(path);

                while (!path_queue.empty())
                {
                    auto cur_path = path_queue.front();
                    path_queue.pop();
                    //cout << "pop path\n";

                    if (cur_path.first.back() == src) //done !
                    {
                        cand_path.push_back(cur_path.first);
                        continue;
                    }

                    auto pt = path_table_ver2[src][cur_path.first.back()];

                    for (auto &cand : pt.cand)
                    {
                        if (cand.hops == 1 && cand.hops == cur_path.second)
                        {
                            auto temp_path = cur_path.first;
                            temp_path.push_back(src);
                            cand_path.push_back(temp_path);
                            //cout << "push source " << src << endl;
                        }
                        else if (cand.hops == cur_path.second)
                        {
                            for (auto &par : cand.parent)
                            {
                                auto temp_path = cur_path.first;
                                //cout << "push parent " << par << endl;
                                temp_path.push_back(par);
                                auto pq = make_pair(temp_path, cur_path.second - 1);
                                path_queue.push(pq);
                            }
                        }
                    }
                }
            }

            /*for (auto &path : cand_path)
            {
                for (auto &p : path)
                {
                    cout << p << " ";
                }
                cout << endl;
            }
            cout << endl;*/

            //try all candidate paths and route the best one
            double best = INT_MAX;
            int index, count = 0;

            /*if (n.id == 3236)
            {
                cout << "\n-----------------------------" << endl;
                cout << "cur sink = " << sb.sink << endl;
                cout << "all candidates : \n";
            }*/

            for (auto &path : cand_path)
            {
                int sink_num;

                //檢查path是否提前連到tree上了導致dummy node
                int check_idx = 1;
                for (int i = 1; i < path.size(); i++)
                {
                    if (sources.count(path[i]) > 0)
                    {
                        check_idx = i; //第一個連到tree的點
                        break;
                    }
                }
                //pop多餘的點
                if (check_idx < path.size() - 1)
                {
                    for (int i = 0; i < path.size() - 1 - check_idx; i++)
                    {
                        path.pop_back();
                    }
                }

                double cost = compute_cost_for_gr2(net[par_net_id], path, sb, sink_num);

                /*if (n.id == 3236)
                {
                    cout << count << " : ";
                    for (auto &p : path)
                    {
                        cout << p << " ";
                    }
                    cout << ", sink num = " << sink_num;
                    cout << ", cost = " << cost << endl;
                }*/

                if (cost < best)
                {
                    best = cost;
                    index = count;
                }

                count++;
            }

            //getchar();

            allpaths.push_back(cand_path[index]);
            n.allpaths.push_back(make_pair(cand_path[index], sb));

            /*if (n.id == 3236)
            {
                cout << "choose " << index << endl;
            }*/

            for (size_t i = 0; i < cand_path[index].size() - 1; i++)
            {
                if (edge_lut.count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
                {
                    edge_lut[make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                    add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);
                    inf_channel_add.push_back(make_pair(cand_path[index][i + 1], cand_path[index][i]));
                }
                //sources.push_back(cand_path[index][i]);
                sources[cand_path[index][i]] = 1;
            }
            //cout << "par = " << par_net_id << ", subnet : " << source << " " << sink << endl;

            routing_subtree(n, cand_path[index]); //add path to routing tree
        }

        //delete_tree(n.rtree_root);
        //n.total_tree_edge = 0;
        //n.rtree_root = NULL;
        //delete_tree(last_rip);
        //routing_tree(n, allpaths);

        //cout << "-----------------\n";
        /*cout << "reroute paths : " << endl;
        for (const auto &path : allpaths)
        {
            for (const auto &p : path)
            {
                cout << p << " ";
            }
            cout << endl;
        }*/

        //after tree
        //cout << "-----------------\n";
        //cout << "after tree : \n";
        compute_edge_weight(n, n.rtree_root); //更新edge weight資訊
        double new_net_cost = comptue_tree_TDM_cost(n.rtree_root);
        new_cost += new_net_cost;

        if (new_cost < old_cost)
        {
            /*
            cout << n.name << " : ";
            cout << "new net cost = " << new_net_cost << endl;
            cout << "old cost = " << old_cost << ", new cost = " << new_cost << endl;
            cout << " ---> update cost !!" << endl;
            cout << endl;
            */

            old_cost = new_cost;
            n.cost = new_net_cost;
            //getchar();
        }
        else //還原
        {
            /*
            cout << n.name << " : ";
            cout << "old cost = " << old_cost << ", new cost = " << new_cost << endl;
            */
            n.allpaths = old_netallpaths;
            delete_tree(n.rtree_root);

            /*if (n.id == 3236)
            {
                for (const auto &path : old_allpaths)
                {
                    for (const auto &p : path)
                    {
                        cout << p << " ";
                    }
                    cout << endl;
                }
            }*/

            routing_tree(n, old_allpaths);

            /*----------------------------------------還原受影響channel----------------------------------------*/
            for (const auto &ch : inf_channel_add) //將加過demand的channel減回去
            {
                sub_channel_demand(ch.first, ch.second);
            }

            for (const auto &ch : inf_channel_sub) //將減過demand的channel加回去
            {
                add_channel_demand(ch.first, ch.second);
            }
            /*-------------------------------------------------------------------------------------------------*/

            compute_edge_weight(n, n.rtree_root);
            //cout << "還原 cost --->" << comptue_tree_TDM_cost(n.rtree_root) + new_cost - new_net_cost << endl;
            //cout << endl;
            //getchar();
        }

        //show_tree(n.rtree_root);
        //cout << endl;
        /*if (n.id == 3236)
            getchar();*/

        //getchar();

        /*if (n.id == 5730)
        {
            cout << "5730 rip channels" << endl;
            for (auto &ch : inf_channel)
            {
                cout << ch.first << "," << ch.second << endl;
            }
            getchar();
        }*/
    }

    //cout << "ok" << endl;
}

void FPGA_Gr::subtree_sink_RR() //SRT RR
{
    double old_cost = total_cost;

    //決定要RR的Net order
    for (auto &n : net)
    {
        double net_avg_skw = n.total_sink_weight / (double)n.sink.size();
        //double edge_weight_penalty = n.total_edge_weight / (double)n.sink.size();
        double edge_weight_penalty = (n.total_edge_weight / (double)n.total_tree_edge) - net_avg_skw;
        //double weight_order = n.total_order / (double)n.sink.size(); //越先被route越先reroute

        n.criticality = net_avg_skw * n.total_tdm + edge_weight_penalty;
        //n.criticality *= weight_order;

        //n.criticality = net_avg_skw * n.total_tdm;
    }
    sort(net.begin(), net.end(), comp_netcrit);

    //start to RR
    for (int i = 0; i < (int)net.size(); i++)
    {
        if (i > 1.0 / 2.0 * net.size()) //挑出criticality大小前面1/2的net來RR
            break;

        auto &n = net[i];

        n.cost = comptue_tree_TDM_cost(n.rtree_root);
        double new_cost = old_cost - n.cost;

        //find max_diff subtree root node (level 1~level 2), level 0 => root
        int rip_fpga_id;
        double max_diff_value = INT_MIN;
        vector<pair<int, int>> inf_channel_sub;
        vector<pair<int, int>> inf_channel_add;

        queue<Tree_Node *> fifo_queue;
        fifo_queue.push(n.rtree_root);

        while (fifo_queue.size() != 0)
        {
            Tree_Node *cur = fifo_queue.front();
            fifo_queue.pop();

            //check tree level是不是前3層(0~2)
            if (cur->fpga_id != n.source) //check 是不是第0層
            {
                if (cur->parent->fpga_id != n.source) //check 是不是第1層
                {
                    if (cur->parent->parent->fpga_id != n.source) //check 是不是第2層
                    {
                        continue;
                    }
                    else //目前位於第2層
                    {
                        double diff_val = diff_weight_es(cur);
                        //cout << "L2 : " << cur->fpga_id;
                        //cout << ", diff_val = " << diff_val << endl;

                        if (diff_val > max_diff_value)
                        {
                            max_diff_value = diff_val;
                            rip_fpga_id = cur->fpga_id;
                        }
                    }
                }
                else //目前位於第1層
                {
                    double diff_val = diff_weight_es(cur);
                    //cout << "L1 : " << cur->fpga_id;
                    //cout << ", diff_val = " << diff_val << endl;

                    if (diff_val > max_diff_value)
                    {
                        max_diff_value = diff_val;
                        rip_fpga_id = cur->fpga_id;
                    }
                }
            }
            else //目前位於第0層
            {
                //cout << "L0 : " << cur->fpga_id << endl;
            }

            for (auto &child : cur->children)
            {
                fifo_queue.push(child);
            }
        }

        //cout << "rip " << n.name << " node F" << rip_fpga_id << endl;

        Tree_Node *node = search_node(n.rtree_root, rip_fpga_id);

        if (node == nullptr || node->fpga_id == n.source)
            continue;

        Tree_Node *node_par = node->parent;

        //cout << "before" << endl;
        //show_tree(n.rtree_root);

        //找出最後要拔掉的點 (這個點以下node都被拔掉，包含這個點)
        Tree_Node *last_rip = node;

        while (node_par->children.size() - 1 == 0 && node_par->fpga_id != n.source)
        {
            last_rip = node_par;
            node_par = node_par->parent;
        }

        //cout << "delete node : " << last_rip->fpga_id << endl;

        //拔
        for (auto it = node_par->children.begin(); it != node_par->children.end(); ++it)
        {
            if ((*it)->fpga_id == last_rip->fpga_id)
            {
                node_par->children.erase(it);
                break;
            }
        }

        pair<int, int> last_chan = make_pair(last_rip->parent->fpga_id, last_rip->fpga_id);
        sub_channel_demand(last_chan.first, last_chan.second);
        inf_channel_sub.push_back(last_chan);
        last_rip->parent = NULL; //delete node parent

        compute_edge_weight(n, n.rtree_root);

        map<int, int> rip_node_lut;                                              //存放所有被拔掉的點
        vector<SubNet> allsubnets;                                               //要reroute的sink
        auto influence = sub_allchannels(n, last_rip, allsubnets, rip_node_lut); //把受影響channel的demand都-1，並記錄下來，以及找出要reroute的sink
        inf_channel_sub.insert(inf_channel_sub.end(), influence.begin(), influence.end());

        /*--------------------------------------backup old allpaths--------------------------------*/
        vector<vector<int>> old_allpaths;
        vector<pair<vector<int>, SubNet>> old_netallpaths;
        for (const auto &apath : n.allpaths)
        {
            old_netallpaths.push_back(apath);
            old_allpaths.push_back(apath.first);
        }
        /*-------------------------------------------------------------------------------------------*/

        //reroute all subnets
        map<pair<int, int>, int> edge_lut;
        map<int, int> sources = ret_all_node_and_edges(n.rtree_root, edge_lut); //rip up後的tree每個點都可當要reroute的sink的source
        int hop_limit = LIMIT_HOP;                                              //最多嘗試與最小hop數差幾個hop的限制條件

        for (auto &sb : allsubnets)
        {
            const auto &par_net_id = sb.parent_net;
            int source = sb.source;
            int sink = sb.sink;

            //check 這個 sink 是不是被 route 過
            if (sources.count(sink) > 0)
            {
                continue;
            }

            //start to routing
            vector<vector<int>> cand_path;
            vector<int> src_candidate;
            int cur_node = source;
            queue<pair<vector<int>, int>> path_queue; //(path, 剩餘hop)
            int minimum_hop;

            int min_hops = INT_MAX;
            for (const auto &s : sources)
            {
                int min = INT_MAX;
                for (const auto &cand : path_table_ver2[s.first][sink].cand)
                {
                    if (cand.hops < min)
                    {
                        min = cand.hops;
                    }
                }
                const int &tmp = min;
                if (tmp < min_hops && tmp != 0)
                    min_hops = tmp;
                //src_candidate.push_back(s.first);
            }

            for (const auto &s : sources)
            {
                if (path_table_ver2[s.first][sink].cand[0].hops == min_hops)
                {
                    src_candidate.push_back(s.first);
                }
            }

            minimum_hop = min_hops;

            for (const auto &src : src_candidate)
            {
                const auto &pt_init = path_table_ver2[src][sink];
                for (const auto &cand : pt_init.cand)
                {
                    int hops = cand.hops;

                    if (hops > minimum_hop + hop_limit)
                    {
                        break;
                    }

                    for (const auto &par : cand.parent)
                    {
                        vector<int> path;
                        path.push_back(sink);
                        path.push_back(par);
                        auto pq = make_pair(path, hops - 1);
                        path_queue.push(pq);
                    }
                }

                while (!path_queue.empty())
                {
                    auto cur_path = path_queue.front();
                    path_queue.pop();

                    if (cur_path.first.back() == src) //done !
                    {
                        cand_path.push_back(cur_path.first);
                        continue;
                    }

                    auto pt = path_table_ver2[src][cur_path.first.back()];

                    for (auto &cand : pt.cand)
                    {
                        if (cand.hops == 1 && cand.hops == cur_path.second)
                        {
                            auto temp_path = cur_path.first;
                            temp_path.push_back(src);
                            cand_path.push_back(temp_path);
                        }
                        else if (cand.hops == cur_path.second)
                        {
                            for (auto &par : cand.parent)
                            {
                                auto temp_path = cur_path.first;
                                temp_path.push_back(par);
                                auto pq = make_pair(temp_path, cur_path.second - 1);
                                path_queue.push(pq);
                            }
                        }
                    }
                }
            }

            //try all candidate paths and route the best one
            double best = INT_MAX;
            int index, count = 0;

            for (auto &path : cand_path)
            {
                int sink_num;

                //檢查path是否提前連到tree上了導致dummy node
                int check_idx = 1;
                for (int i = 1; i < path.size(); i++)
                {
                    if (sources.count(path[i]) > 0)
                    {
                        check_idx = i; //第一個連到tree的點
                        break;
                    }
                }
                //pop多餘的點
                if (check_idx < path.size() - 1)
                {
                    for (int i = 0; i < path.size() - 1 - check_idx; i++)
                    {
                        path.pop_back();
                    }
                }

                double cost = compute_cost_for_gr2(net[par_net_id], path, sb, sink_num);

                if (cost < best)
                {
                    best = cost;
                    index = count;
                }

                count++;
            }

            n.allpaths.push_back(make_pair(cand_path[index], sb));

            for (size_t i = 0; i < cand_path[index].size() - 1; i++)
            {
                if (edge_lut.count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
                {
                    edge_lut[make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                    add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);
                    inf_channel_add.push_back(make_pair(cand_path[index][i + 1], cand_path[index][i]));
                }
                sources[cand_path[index][i]] = 1;
            }

            routing_subtree(n, cand_path[index]); //add path to routing tree
        }

        compute_edge_weight(n, n.rtree_root); //更新edge weight資訊
        double new_net_cost = comptue_tree_TDM_cost(n.rtree_root);
        new_cost += new_net_cost;

        if (new_cost < old_cost)
        {
            old_cost = new_cost;
            n.cost = new_net_cost;
        }
        else //還原
        {
            n.allpaths = old_netallpaths;
            delete_tree(n.rtree_root);
            routing_tree(n, old_allpaths);

            /*----------------------------------------還原受影響channel----------------------------------------*/
            for (const auto &ch : inf_channel_add) //將加過demand的channel減回去
            {
                sub_channel_demand(ch.first, ch.second);
            }

            for (const auto &ch : inf_channel_sub) //將減過demand的channel加回去
            {
                add_channel_demand(ch.first, ch.second);
            }
            /*-------------------------------------------------------------------------------------------------*/

            compute_edge_weight(n, n.rtree_root);
        }
    }
}

void FPGA_Gr::subtree_RR() //SBT RR
{
    //找sink點拔 （）
}

void FPGA_Gr::check_result()
{
    //check all nets have been routed correctly
    cout << "check all signals have been routed correctly...";
    for (const auto &n : net)
    {
        if (n.rtree_root->fpga_id != n.source) //檢查tree的root是否為net的source
        {
            cout << "Error" << endl;
            cout << n.name << "'s source = " << n.source << "<----->" << n.rtree_root->fpga_id << " = tree root" << endl;
            //exit(1);
        }

        list<int> net_terminal;
        queue<Tree_Node *> fifo_queue;
        fifo_queue.push(n.rtree_root);

        net_terminal.push_back(n.source);
        for (const auto &sk : n.sink)
        {
            net_terminal.push_back(sk.id);
        }

        while (!fifo_queue.empty())
        {
            Tree_Node *cur = fifo_queue.front();
            fifo_queue.pop();
            net_terminal.remove(cur->fpga_id);

            for (const auto &chi : cur->children)
            {
                // check相連的child元素是否為parent鄰居
                bool find = false;
                for (const auto &nbr : fpga[cur->fpga_id].nbr_pair)
                {
                    if (chi->fpga_id == nbr.first)
                    {
                        find = true;
                        break;
                    }
                }

                if (!find)
                {
                    cout << "Error" << endl;
                    cout << n.name << " : " << chi->fpga_id << " is not the neighbor of " << cur->fpga_id << " !\n";
                    //exit(1);
                }

                //check edge weight是否正確
                if (cur->edge_weight < chi->sink_weight)
                {
                    cout << "Error" << endl;
                    cout << n.name << " : parent edge_weight = " << cur->edge_weight << " must >= child sink weight = " << chi->sink_weight << endl;
                    //exit(1);
                }

                if (cur->edge_weight != cur->max_value)
                {
                    cout << "Error" << endl;
                    cout << n.name << " : " << cur->fpga_id << "'s edge weight error or max value error" << endl;
                    //exit(1);
                }

                fifo_queue.push(chi);
            }
        }

        if (!net_terminal.empty())
        {
            cout << "Error" << endl;
            cout << n.name << " : ";
            for (const auto &id : net_terminal)
            {
                cout << id << " ";
            }
            cout << "did not be routed" << endl;
            //exit(1);
        }
    }
    cout << "OK" << endl;
}

void FPGA_Gr::initial_route_result()
{
    //compute history cost
    int ch_num = channel_demand.size();
    double avg_use = (double)total_demand / (double)ch_num;

    for (auto &ch : map_to_channel)
    {
        ch.second->history_used[0] = ch.second->history_used[1] = 0;
    }

    for (auto &chd : channel_demand)
    {
        int direct = (chd.first.first < chd.first.second) ? 0 : 1; //min-->max : 0, max-->min : 1
        int cap = channel_capacity[chd.first];
        auto ch_name = get_channel_name(chd.first.first, chd.first.second);
        auto ch = map_to_channel[ch_name];

        auto demand = chd.second;
        int cur_channel_tdm = (int)ceil((double)chd.second / (double)cap);
        double his_cost = 0.0;
        double times = 1; //控制map的範圍-->ex. times = 2 --> map to [0,1]*2 + 1 = [1,3]

        if (cur_channel_tdm > 1)
        {
            for (const auto &node : ch->passed_nets[direct])
            {
                double norm_cost = (((node->signal_weight - minsgw) / (maxsgw - minsgw)) * times + 1);
                his_cost += norm_cost;
            }

            his_cost = his_cost / (double)cap;
        }
        else
        {
            his_cost = 0;
        }

        //his_cost = cur_channel_tdm;

        /*if (cur_channel_tdm > avg_tdm_ratio)
        {
            his_cost = cur_channel_tdm - avg_tdm_ratio;
        }
        else
        {
            his_cost = 0;
        }*/

        ch->history_used[direct] += his_cost;
    }

    //sorted net by tree edge num
    //sort(net.begin(), net.end(), comp_edges);

    //delete route tree
    for (auto &n : net)
    {
        delete_tree(n.rtree_root);
        n.total_tree_edge = 0;
        n.rtree_root = NULL;
    }

    //initial channel demand
    for (auto &ch : channel_demand)
    {
        ch.second = 0;
    }

    total_demand = 0;
    mintdm = INT_MAX;
    maxtdm = 0;
}

void FPGA_Gr::show_congestion_map()
{
    for (const auto &ch : map_to_channel)
    {
        if (ch.second->capacity == 0)
            continue;

        cout << "Channel Name : (" << ch.first.first << ", " << ch.first.second << ")\n";
        cout << "TDM_0 : " << channel_TDM(ch.first.first, ch.first.second) << endl;
        cout << "TDM_1 : " << channel_TDM(ch.first.second, ch.first.first) << endl;

        /*
        cout << "Nets_0: ";
        for (const auto &n : ch.second->passed_nets[0])
        {
            cout << n->name << ", ";
        }
        cout << endl;
        cout << "Nets_1: ";
        for (const auto &n : ch.second->passed_nets[1])
        {
            cout << n->name << ", ";
        }
        cout << endl;
        */

        cout << endl;
    }
}

void FPGA_Gr::congestion_RR() //CRR
{
    //sort congestion map
    for (const auto &cm : congestion_map)
    {
        cong_map_vec.push_back(make_pair(cm.first, cm.second));
    }
    sort(cong_map_vec.begin(), cong_map_vec.end(), comp_conj_vec_map);

    list<Net *> rip_net_set;
    int rip_num = cong_map_vec.size() * 0.1;

    //cout << "\n\tripped channels : " << endl;
    
    for (int i = 0; i < rip_num; i++)
    {
        //int dir = (cong_map_vec[i].first.first > cong_map_vec[i].first.second) ? 1 : 0;
        auto ch_name = cong_map_vec[i].first;
        auto ch = map_to_channel[ch_name];

        old_map_vec[ch_name] = 1;
        add_ch_RRtimes(ch_name);

        //cout << "\t(" << ch_name.first << ", " << ch_name.second << ")" << endl;

        for (auto &n : ch->passed_nets[0])
        {
            if (n->chan_penalty.count(ch_name) > 0)
            {
                n->chan_penalty[ch_name] += 0.05;
            }
            else
            {
                n->chan_penalty[ch_name] = 1.05; //防止再走回原channel
            }

            //rip_net_set.push_back(n);

            if (round == 1)
            {
                rip_net_set.push_back(n);
                n->ripped[round - 1] = true;
            }

            if (round > 1)
            {
                if (n->ripped[round - 2] == false)
                {
                    rip_net_set.push_back(n);
                    n->ripped[round - 1] = true;    
                }
            }
        }

        for (auto &n : ch->passed_nets[1])
        {
            //rip_net_set.push_back(n);

            if (round == 1)
            {
                rip_net_set.push_back(n);
                n->ripped[round - 1] = true;
            }

            if (round > 1)
            {
                if (n->ripped[round - 2] == false)
                {
                    rip_net_set.push_back(n);
                    n->ripped[round - 1] = true;    
                }
            }
        }
    }

    //rip-up nets and compute criticality
    vector<pair<Net *, double>> ripped_net; //net, crit
    rip_net_set.sort();
    rip_net_set.unique();

    cout << "\n\t#ripped signals = " << rip_net_set.size()
         << "(" << (double)rip_net_set.size() / (double)net.size() * 100 << "%)" << endl;

    int repeated = 0;
    auto tmp_rep = repeat_RR;
    repeat_RR.clear();
    repeat_RR.resize(net.size());

    for (auto &n : rip_net_set)
    {
        rip_up_net(*n);

        if (tmp_rep[n->id] == 1)
        {
            repeated++;
        }

        repeat_RR[n->id] = 1;

        double net_avg_skw = n->total_sink_weight / (double)n->sink.size();
        double edge_weight_penalty = (n->total_edge_weight / (double)n->total_tree_edge) - net_avg_skw;
        double criticality = net_avg_skw * n->total_tdm + edge_weight_penalty;

        ripped_net.push_back(make_pair(n, criticality));
    }

    cout << "\trepeat num = " << repeated << endl;

    sort(ripped_net.begin(), ripped_net.end(), comp_by_second); //net order decision (sorted by criticality)

    //reroute net
    for (const auto &rip_net_crit : ripped_net)
    {
        auto n_ptr = rip_net_crit.first; //point to ripped_net
        reroute_net(n_ptr);
        compute_edge_weight(*n_ptr, n_ptr->rtree_root);
        //show_tree(n_ptr->rtree_root);
    }

    //reset congestion map
    for (auto &cm : congestion_map)
    {
        cm.second = 0;
    }
    cong_map_vec.clear();

    round++;
}

void FPGA_Gr::set_after_conj_cost()
{
    /*after_conj_cost.resize(10);
    for (int i = 0; i < 10; i++)
    {
        auto conj_cost = congestion_map[make_pair(cong_map_vec[i].first.first, cong_map_vec[i].first.second)];
        after_conj_cost[i] = conj_cost;
    }

    cong_map_vec.clear();*/
}

void FPGA_Gr::update_history_cost()
{   
    for (auto &chd : channel_demand)
    {
        int direct = (chd.first.first < chd.first.second) ? 0 : 1; //min-->max : 0, max-->min : 1
        int cap = channel_capacity[chd.first];
        auto ch_name = get_channel_name(chd.first.first, chd.first.second);
        auto ch = map_to_channel[ch_name];

        int cur_channel_tdm = (int)ceil((double)chd.second / (double)cap);
        double his_cost = 0.0;

        if (cur_channel_tdm > 1)
        {
            /*for (int i = 0; i < 10; i++)
            {
                if(after_conj_cost[i] >= cong_map_vec[i].second)
                {
                    ch->history_penalty[direct] += 0.2;
                    break;
                }
            }*/

            ch->history_cost[direct] = cur_channel_tdm * ch->history_penalty[direct];
        }
        else
        {
            ch->history_cost[direct] = 0;
        }
    }
}

void FPGA_Gr::reroute_net(Net *n)
{
    //subnetbased = true;
    int hop_limit = LIMIT_HOP; //最多嘗試與最小hop數差幾個hop的限制條件
    vector<pair<SubNet, int>> subnet_order;
    vector<vector<int>> allpaths; //紀錄每個net的所有subpath
    map<pair<int, int>, int> edge_lut;
    map<int, int> sources;

    //算出subnet weight決定routing order
    for (auto &sb : n->sbnet)
    {
        pair<SubNet, int> temp;
        const int &src = sb.source;
        const int &sink = sb.sink;
        const int &hops = path_table_ver2[src][sink].cand[0].hops;
        int weight = sb.weight;
        temp = make_pair(sb, weight);
        subnet_order.push_back(temp);
    }
    sort(subnet_order.begin(), subnet_order.end(), comp_sbnetcost);

    double total_time = 0.0;

    //start to route subnet
    int sub_order = 0;
    for (auto &sb : subnet_order)
    {
        auto t2 = clock();
        int source = sb.first.source;
        int sink = sb.first.sink;

        //check 這個 sink 是不是被 route 過
        if (sources.count(sink) > 0)
        {
            continue;
        }

        //start to routing
        vector<vector<int>> cand_path;
        vector<int> src_candidate;
        int cur_node = source;
        queue<pair<vector<int>, int>> path_queue; //(path, 剩餘hop)
        int minimum_hop;

        if (sources.size() == 0)
        {
            sources[source] = 1;
            src_candidate.push_back(source);
            minimum_hop = path_table_ver2[source][sink].cand[0].hops;
        }
        else
        {
            int min_hops = INT_MAX;
            for (const auto &s : sources)
            {
                int min = INT_MAX;
                for (const auto &cand : path_table_ver2[s.first][sink].cand)
                {
                    if (cand.hops < min)
                    {
                        min = cand.hops;
                    }
                }
                const int &tmp = min;
                if (tmp < min_hops && tmp != 0)
                    min_hops = tmp;
            }

            for (const auto &s : sources)
            {
                int min = path_table_ver2[s.first][sink].cand[0].hops;
                if (min == min_hops)
                {
                    src_candidate.push_back(s.first);
                }
            }

            minimum_hop = min_hops;
        }

        for (const auto &src : src_candidate)
        {
            const auto &pt_init = path_table_ver2[src][sink];
            for (const auto &cand : pt_init.cand)
            {
                int hops = cand.hops;

                if (hops > minimum_hop + hop_limit)
                {
                    continue;
                }

                for (const auto &par : cand.parent)
                {
                    vector<int> path;
                    path.push_back(sink);
                    path.push_back(par);
                    auto pq = make_pair(path, hops - 1);
                    path_queue.push(pq);
                }
            }

            while (!path_queue.empty())
            {
                auto cur_path = path_queue.front();
                path_queue.pop();

                if (cur_path.first.back() == src) //done !
                {
                    cand_path.push_back(cur_path.first);
                    continue;
                }

                auto pt = path_table_ver2[src][cur_path.first.back()];

                for (auto &cand : pt.cand)
                {
                    if (cand.hops == 1 && cand.hops == cur_path.second)
                    {
                        auto temp_path = cur_path.first;
                        temp_path.push_back(src);
                        cand_path.push_back(temp_path);
                    }
                    else if (cand.hops == cur_path.second)
                    {
                        for (auto &par : cand.parent)
                        {
                            auto temp_path = cur_path.first;
                            temp_path.push_back(par);
                            auto pq = make_pair(temp_path, cur_path.second - 1);
                            path_queue.push(pq);
                        }
                    }
                }
            }
        }

        //try all candidate paths and route the best one
        double best = INT_MAX;
        int index, count = 0;

        for (auto &path : cand_path)
        {
            int sink_num;

            //檢查path是否提前連到tree上了導致dummy node
            int check_idx = 1;
            for (int i = 1; i < path.size(); i++)
            {
                if (sources.count(path[i]) > 0)
                {
                    check_idx = i; //第一個連到tree的點
                    break;
                }
            }
            //pop多餘的點
            if (check_idx < path.size() - 1)
            {
                for (int i = 0; i < path.size() - 1 - check_idx; i++)
                {
                    path.pop_back();
                }
            }

            double cost = compute_cost_for_CCR(*n, path, sb.first, sink_num);

            if (cost < best)
            {
                best = cost;
                index = count;
            }

            count++;
        }

        //getchar();

        allpaths.push_back(cand_path[index]);

        for (size_t i = 0; i < cand_path[index].size() - 1; i++)
        {
            if (edge_lut.count(make_pair(cand_path[index][i + 1], cand_path[index][i])) == 0)
            {
                edge_lut[make_pair(cand_path[index][i + 1], cand_path[index][i])] = 1;
                add_channel_demand(cand_path[index][i + 1], cand_path[index][i]);

                //不確定對不對
                auto ch = map_to_channel[get_channel_name(cand_path[index][i + 1], cand_path[index][i])];
                int dir = (cand_path[index][i + 1] > cand_path[index][i]) ? 1 : 0;
                ch->passed_nets[dir].push_back(n);
            }
            sources[cand_path[index][i]] = 1;
        }

        total_time += ((double)(clock() - t2) / (double)CLOCKS_PER_SEC);
        routing_subtree(*n, cand_path[index]); //add path to routing tree
    }
}

void FPGA_Gr::rip_up_net(Net &n)
{
    queue<Tree_Node *> fifo_queue;
    fifo_queue.push(n.rtree_root);

    if (n.rtree_root == NULL)
        return;

    while (fifo_queue.size() != 0)
    {
        Tree_Node *cur = fifo_queue.front();
        fifo_queue.pop();

        if (cur->children.size() == 0)
            continue;

        for (const auto &child : cur->children)
        {
            sub_channel_demand(cur->fpga_id, child->fpga_id);

            //不確定對不對
            int dir = (cur->fpga_id > child->fpga_id) ? 1 : 0;
            auto ch = map_to_channel[get_channel_name(cur->fpga_id, child->fpga_id)];
            Net *rm_net;

            for (auto &ch_passed : ch->passed_nets[dir])
            {
                if (ch_passed->name == n.name)
                {
                    rm_net = ch_passed;
                    break;
                }
            }
            ch->passed_nets[dir].remove(rm_net);

            fifo_queue.push(child);
        }
        delete cur;
    }

    n.total_tree_edge = 0;
    n.rtree_root = NULL;
}

double FPGA_Gr::compute_cost_for_CCR(Net &n, const vector<int> &path, const SubNet &sbnet, int &sink_num)
{
    double cost = 0.0, cost_path = 0.0, appr_tdm = 0.0;
    double weight = sbnet.weight;
    double hop_num = path.size() - 1;
    double total_used = 0.0, net_cost = 0.0;
    double total_his_cost = 0.0;
    double total_weight = 0.0;
    double cost_par = 0.0;
    sink_num = 1;

    map<int, int> sink_lut;
    for (const auto &sk : n.sink)
    {
        sink_lut[sk.id] = sk.weight;
    }

    for (size_t i = 0; i < path.size() - 1; i++)
    {
        int cap = channel_capacity[make_pair(path[i + 1], path[i])];

        cap = (cap == 0) ? 1 : cap;

        //double sig_weight = n.edge_crit[make_pair(path[i + 1], path[i])];
        const int &direct = (path[i + 1] < path[i]) ? 0 : 1; //min-->max : 0, max-->min : 1
        double ch_used = channel_used(path[i + 1], path[i]);

        double before_tdm = (double)(ch_used) / (double)cap;
        double appr_tdm = (double)(ch_used + 1) / (double)cap; //src to sink appr. tdm

        auto ch_name = get_channel_name(path[i], path[i + 1]);
        auto ch = map_to_channel[ch_name];
        double his_cost = ch->history_cost[direct];

        //check weight
        if (i > 0)
        {
            //check if current node is a sink ?
            if (sink_lut.count(path[i]) > 0)
            {
                sink_num++;
                if (sink_lut[path[i]] > weight)
                {
                    weight = sink_lut[path[i]];
                }
            }
        }

        total_weight += weight;
        int wirelength = i + 1;

        if (ceil(appr_tdm) <= ceil(before_tdm))
        {
            ch_used = 0;
        }

        //計算net的history channel penalty
        double nch_penalty = 1;
        if (n.chan_penalty.count(ch_name) > 0) //存在penalty
        {
            nch_penalty = n.chan_penalty[ch_name];
        }
        
        
        appr_tdm = (appr_tdm < 1) ? 1 : appr_tdm;
        double base_cost = weight * appr_tdm;
        double congestion_cost = cost_par + (nch_penalty * base_cost);
        double cost_cur = congestion_cost;

        cost_par = cost_cur;
        cost_path += cost_cur;
    }

    if (sink_lut.count(path.back()) > 0)
        sink_num++;

    cost = cost_path;
    cost /= (double)sink_num;

    return cost;
}

void FPGA_Gr::add_ch_RRtimes(pair<int, int> ch_name)
{
    if (RRtimes.count(ch_name) == 0)
    {
        RRtimes[ch_name] = 1;
    }
    else
    {
        RRtimes[ch_name]++;
    }
}