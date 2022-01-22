//
// Created by 付聪 on 2017/6/21.
//

#include <efanna2e/index_nsg.h>
#include <efanna2e/util.h>
#include <chrono>
#include <string>
#include <unordered_set>
#include <sched.h>

using namespace std;

cpu_set_t  mask;
inline void assignToThisCore(int core_id){
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
}



// void load_data(char* filename, float*& data, unsigned& num,
//                unsigned& dim) {  // load data with sift10K pattern
//   std::ifstream in(filename, std::ios::binary);
//   if (!in.is_open()) {
//     std::cout << "open file error" << std::endl;
//     exit(-1);
//   }
//   in.read((char*)&dim, 4);
//   // std::cout<<"data dimension: "<<dim<<std::endl;
//   in.seekg(0, std::ios::end);
//   std::ios::pos_type ss = in.tellg();
//   size_t fsize = (size_t)ss;
//   num = (unsigned)(fsize / (dim + 1) / 4);
//   data = new float[(size_t)num * (size_t)dim];

//   in.seekg(0, std::ios::beg);
//   for (size_t i = 0; i < num; i++) {
//     in.seekg(4, std::ios::cur);
//     in.read((char*)(data + i * dim), dim * 4);
//   }
//   in.close();
// }

void load_data(char* filename, float*& data, unsigned& num,
               unsigned& dim) {  // load data with sift10K pattern
  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open()) {
    std::cout << "open file error" << std::endl;
    exit(-1);
  }

  in.read((char *) &num, sizeof(unsigned));
  in.read((char *) &dim, sizeof(unsigned));
  data = new float[num * dim * sizeof(float)];

  in.read((char *) data, num * dim * sizeof(float));
  in.close();
}

template<typename data_T>
void LoadBinToArray(std::string& file_path, data_T *data_m, uint32_t nums, uint32_t dims, bool non_header = false){
    std::ifstream file_reader(file_path.c_str(), std::ios::binary);
    if (!non_header){
        uint32_t nums_r, dims_r;
        file_reader.read((char *) &nums_r, sizeof(uint32_t));
        file_reader.read((char *) &dims_r, sizeof(uint32_t));
        if ((nums != nums_r) || (dims != dims_r)){
            printf("Error, file size is error, nums_r: %u, dims_r: %u\n", nums_r, dims_r);
            exit(1);
        }
    }

    file_reader.read((char *) data_m, nums * dims * sizeof(data_T));
    file_reader.close();
    printf("Load %u * %u Data from %s done.\n", nums, dims, file_path.c_str());
}

float comput_recall(std::vector<std::vector<unsigned>> &res, unsigned *gt, unsigned &qsize, unsigned &k){
  size_t correct = 0;
  size_t total = 0;
  
  for (size_t qi = 0; qi < qsize; qi++){
    std::unordered_set<unsigned> g;
    for (size_t i = 0; i < k; i++)
      g.insert(gt[qi * k + i]);
    total += res[qi].size();

    for (unsigned res_i : res[qi]){
      if (g.find(res_i) != g.end()){
        correct++;
      }
    }
  }
  return (float)correct / total;
}

void save_result(const char* filename,
                 std::vector<std::vector<unsigned> >& results) {
  std::ofstream out(filename, std::ios::binary | std::ios::out);

  for (unsigned i = 0; i < results.size(); i++) {
    unsigned GK = (unsigned)results[i].size();
    out.write((char*)&GK, sizeof(unsigned));
    out.write((char*)results[i].data(), GK * sizeof(unsigned));
  }
  out.close();
}
int main(int argc, char** argv) {
  if (argc != 8) {
    std::cout << argv[0]
              << " data_file query_file nsg_path search_L search_K result_path"
              << std::endl;
    exit(-1);
  }
  float* data_load = NULL;
  unsigned points_num, dim;
  load_data(argv[1], data_load, points_num, dim);
  float* query_load = NULL;
  unsigned query_num, query_dim;
  load_data(argv[2], query_load, query_num, query_dim);
  assert(dim == query_dim);

  unsigned L = (unsigned)atoi(argv[4]);
  unsigned K = (unsigned)atoi(argv[5]);

  if (L < K) {
    std::cout << "search_L cannot be smaller than search_K!" << std::endl;
    exit(-1);
  }

  // data_load = efanna2e::data_align(data_load, points_num, dim);//one must
  // align the data before build query_load = efanna2e::data_align(query_load,
  // query_num, query_dim);
  efanna2e::IndexNSG index(dim, points_num, efanna2e::FAST_L2, nullptr);
  index.Load(argv[3]);
  index.OptimizeGraph(data_load);

  efanna2e::Parameters paras;

  // load gt
  unsigned *gt_all = new unsigned[query_num * 100]();
  unsigned *gt_k = new unsigned[query_num * K]();
  std::string path_gt = std::string(argv[6]);
  LoadBinToArray<unsigned>(path_gt, gt_all, query_num, 100);
  for (size_t i = 0; i < query_num; i++)
    memcpy(gt_k + i * K, gt_all + i * 100, K * sizeof(unsigned));
  delete[] gt_all;

  std::string log_output = std::string(argv[7]);
  std::ofstream log_writer(log_output.c_str(), std::ios::trunc);
  assignToThisCore(27);

  // set L
  std::vector<unsigned> efs;
  for (int i = K; i < 40; i += 5) {
    efs.push_back(i);
  }
  for (int i = 40; i < 100; i += 10) {
    efs.push_back(i);
  }
  for (int i = 100; i <= 500; i += 100) {
    efs.push_back(i);
  }

  log_writer << "R@" << std::to_string(K) << ",qps" << std::endl;
  for (unsigned L_s: efs){

    paras.Set<unsigned>("L_search", L_s);
    paras.Set<unsigned>("P_search", L_s);

    std::vector<std::vector<unsigned> > res(query_num);
    for (unsigned i = 0; i < query_num; i++) res[i].resize(K);

    auto s = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < query_num; i++) {
      index.SearchWithOptGraph(query_load + i * dim, K, paras, res[i].data());
    }
    double time_per_query = std::chrono::duration<double>(std::chrono::steady_clock::now() - s).count() / query_num;

    float recall = comput_recall(res, gt_k, query_num, K);

    std::cout << recall << "\t" << (1.0 / time_per_query) << std::endl;
    log_writer << recall << "," << (1.0 / time_per_query) << std::endl;

    // std::cout << "search time: " << diff.count() << "\n";
  }
  log_writer.close();
  // save_result(argv[6], res);

  return 0;
}
