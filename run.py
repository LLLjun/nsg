import os

path_efanna = "/home/usr-xkIJigVq/vldb/efanna_graph"
path_nsg = "/home/usr-xkIJigVq/vldb/nsg"
stage = "search"

datasets = ["deep"]
datasize_list = [1]
k_list = [10]

for dataname in datasets:
    for datasize in datasize_list:
        if (dataname == "gist" and datasize == 10):
            continue

        for k in k_list:

            root_dataset = "/home/usr-xkIJigVq/DataSet/" + dataname
            path_index = path_nsg + "/graphindex/" + dataname
            path_output = path_nsg + "/output/" + dataname
            if os.path.exists(path_index) is False:
                os.mkdir(path_index)
            if os.path.exists(path_output) is False:
                os.mkdir(path_output)

            # default deep
            basedata_file = root_dataset + "/" + dataname + str(datasize) + "m/base." + str(datasize) + "m.fbin"
            gt_file = root_dataset + "/" + dataname + str(datasize) + "m/groundtruth." + str(datasize) + "m.bin"
            query_file = root_dataset + "/query.public.10K.fbin"
            if dataname == "sift":
                basedata_file = root_dataset + "/" + dataname + str(datasize) + "m/base." + str(datasize) + "m.u8bin"
                query_file = root_dataset + "/query.public.10K.u8bin"
            if dataname == "turing":
                query_file = root_dataset + "/query100K.fbin"
            if dataname == "gist":
                basedata_file = root_dataset + "/gist_base.fvecs"
                gt_file = root_dataset + "/gist_groundtruth.ivecs"
                query_file = root_dataset + "/gist_query.fvecs"
                
            # efanna
            K = 200
            L = 200
            iter = 10
            S = 10
            R = 100
            if dataname == "gist":
                K = 400
                L = 400
                iter = 12
                S = 15
                R = 100
            
            knn_unique_name = dataname + str(datasize) + "m_K" + str(K) + "_L" + str(L) + "_S" + str(S) + "_R" + str(R)
            knn_graph = path_index + "/" + knn_unique_name + ".knn_graph"
            build_knn = path_output + "/" + knn_unique_name + "_build_knn_graph.txt"

            if stage == "build" or stage == "both":
                os.system("cd " + path_efanna + " && make -j && tests/test_nndescent " + basedata_file + " " + knn_graph + " " + \
                            str(K) + " " + str(L) + " " + str(iter) + " " + str(S) + " " + str(R) + " > " + build_knn)


            # nsg
            L_nsg_c = 40
            R_nsg_c = 50
            C_nsg_c = 500
            if dataname == "gist":
                L_nsg_c = 60
                R_nsg_c = 70
                C_nsg_c = 500
            
            nsg_unique_name = dataname + str(datasize) + "m_L" + str(L_nsg_c) + "_C" + str(C_nsg_c) + "_R" + str(R_nsg_c)
            nsg_graph = path_index + "/" + nsg_unique_name + ".nsg"
            build_nsg = path_output + "/" + nsg_unique_name + "_build_nsg.txt"
            search_nsg = path_output + "/" + nsg_unique_name + "_k" + str(k) + ".csv"

            L_nsg_s = 10000

            if stage == "build" or stage == "both":
                os.system("cd " + path_nsg + "/build" + " && make -j && tests/test_nsg_index " + basedata_file + " " + knn_graph + " " + \
                        str(L_nsg_c) + " " + str(R_nsg_c) + " " + str(C_nsg_c) + " " + nsg_graph + " > " + build_nsg)

            if stage == "search" or stage == "both":
                os.system("cd " + path_nsg + "/build" + " && make -j && tests/test_nsg_optimized_search " + \
                        basedata_file + " " + query_file + " " + nsg_graph + " " + \
                        str(L_nsg_s) + " " + str(k) + " " + gt_file + " " + search_nsg)