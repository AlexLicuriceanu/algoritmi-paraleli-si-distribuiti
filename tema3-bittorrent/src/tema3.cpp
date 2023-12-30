#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <tuple>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

using namespace std;

typedef struct {
    int rank;
    int num_clients;
    vector<int> requested_files;
    vector<int> file_sizes;
} download_thread_arg_t;

typedef struct {
    int rank;
    int num_clients;
    vector<vector<string>> files;
    vector<int> file_sizes;
} upload_thread_arg_t;

typedef struct {
    int code;
    int file_index;
    int segment_index;
    int files[MAX_FILES][MAX_CHUNKS];
} tracker_message_t;

typedef struct {
    int file_index;
    int segment_index;
} peer_message_t;

/**
 * Converts the file name to an index.
*/
int name_to_index(string filename) {
    string number;

    for (char c : filename) {
        if (isdigit(c)) {
            number += c;
        }
    }

    return stoi(number) - 1;
}


/**
 * Converts a given index to a file name.
*/
string index_to_name(int index) {
    return "file" + to_string(index+1);
}


/**
 * Outputs the hashes of the requested files.
*/
void write_output_file(int rank, int file_index, int file_size, vector<string>& segments) {

    string filename = "client" + to_string(rank) + "_" + index_to_name(file_index);

    ofstream file(filename);

    for (int i = 0; i < file_size; i++) {
        file << segments[i] << "\n";
    }

    file.close();
}


/**
 * Create an MPI datatype to use when sending a tracker_message_t struct.
*/
MPI_Datatype create_tracker_message_datatype() {

    MPI_Datatype tracker_message_datatype;
    int block_lengths[4] = {1, 1, 1, MAX_FILES * MAX_CHUNKS};
    MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint offsets[4];
    offsets[0] = offsetof(tracker_message_t, code);
    offsets[1] = offsetof(tracker_message_t, file_index);
    offsets[2] = offsetof(tracker_message_t, segment_index);
    offsets[3] = offsetof(tracker_message_t, files);

    MPI_Type_create_struct(4, block_lengths, offsets, types, &tracker_message_datatype);
    MPI_Type_commit(&tracker_message_datatype);

    return tracker_message_datatype;
}


/**
 * Create an MPI datatype to use when sending a peer_message_t struct.
*/
MPI_Datatype create_peer_message_datatype() {
    MPI_Datatype peer_message_datatype;
    int block_lengths[2] = {1, 1};
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};

    MPI_Aint offsets[2];
    offsets[0] = offsetof(peer_message_t, file_index);
    offsets[1] = offsetof(peer_message_t, segment_index);

    MPI_Type_create_struct(2, block_lengths, offsets, types, &peer_message_datatype);
    MPI_Type_commit(&peer_message_datatype);

    return peer_message_datatype;
}


/**
 * Thread function that handles downloading segments from other peers. 
*/
void *download_thread_func(void *arg)
{
    // Unpack the arguments.
    download_thread_arg_t* download_arg = (download_thread_arg_t*) arg;
    int rank = download_arg->rank;
    int num_clients = download_arg->num_clients;
    vector<int> requested_files = download_arg->requested_files;
    vector<int> file_sizes = download_arg->file_sizes;

    // This will store downloaded hashes for each requested file.
    vector<vector<string>> files(MAX_FILES, vector<string>(MAX_CHUNKS));

    // ID of the client that was used for the previous hash download.
    int previous_peer = -1;


    // When a client doesn't want to download any files,
    // signal the tracker that this client has finished
    // downloading.

    int num_requested_files = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (requested_files[i] != 0) {
            num_requested_files++;
        } 
    }
    
    if (num_requested_files == 0) {
        tracker_message_t tracker_message;
        tracker_message.code = 8;

        // Signal the tracker that this client has finished all downloads.
        MPI_Ssend(&tracker_message, 1, create_tracker_message_datatype(), TRACKER_RANK, 8, MPI_COMM_WORLD);
        return NULL;
    }

    while (true) {

        for (int i = 0; i < MAX_FILES; i++) {

            if (requested_files[i] == 0) {
                continue;
            }


            // Request the list of peers from the tracker.
            int peers[num_clients][MAX_CHUNKS];
            memset(peers, 0, sizeof(peers));

            tracker_message_t tracker_message;
            tracker_message.code = 3;
            tracker_message.file_index = i;

            MPI_Status s;

            MPI_Ssend(&tracker_message, 1, create_tracker_message_datatype(), TRACKER_RANK, 3, MPI_COMM_WORLD);
            MPI_Recv(peers, num_clients * MAX_CHUNKS, MPI_INT, TRACKER_RANK, 3, MPI_COMM_WORLD, &s);


            // Find a segment that the client needs.
            int segment_index = -1;

            for (int j = 0; j < file_sizes[i]; j++) {
                if (files[i][j].size() != HASH_SIZE) {
                    segment_index = j;
                    break;
                }
            }

            if (segment_index == -1) {
                continue;
            }


            // Select a client that has this segment, but find a different one than the
            // client used for the previous download (if it exists).
            int client_rank = previous_peer;

            for (int j = 0; j < num_clients; j++) {
                if (peers[j][segment_index] != 0 && j+1 != client_rank) {

                    client_rank = j + 1;
                    previous_peer = client_rank;
                    break;
                }
            }

            // No client was found for this segment, skip.
            if (client_rank == -1) {
                continue;
            }


            // Request the hash from the selected client.
            peer_message_t peer_message;
            peer_message.file_index = i;
            peer_message.segment_index = segment_index;

            MPI_Ssend(&peer_message, 1, create_peer_message_datatype(), client_rank, 4, MPI_COMM_WORLD);

            char hash[HASH_SIZE+1] = {0};
            MPI_Recv(hash, HASH_SIZE, MPI_CHAR, client_rank, 5, MPI_COMM_WORLD, &s);

            // Save the received hash.
            files[i][segment_index] = hash;
            
            // Tell the tracker that this client now has this hash.
            tracker_message.code = 7;
            tracker_message.file_index = i;
            tracker_message.segment_index = segment_index;

            MPI_Ssend(&tracker_message, 1, create_tracker_message_datatype(), TRACKER_RANK, 7, MPI_COMM_WORLD);

            // Check if this file is complete.
            bool complete = true;

            for (int j = 0; j < file_sizes[i]; j++) {
                if (files[i][j].size() != HASH_SIZE) {
                    complete = false;
                    break;
                }
            }

            if (complete) {
    
                // Signal the tracker that the client has finished downloading this file.
                tracker_message_t tracker_message;
                tracker_message.code = 6;
                tracker_message.file_index = i;
                
                MPI_Ssend(&tracker_message, 1, create_tracker_message_datatype(), TRACKER_RANK, 6, MPI_COMM_WORLD);

                // This file is no longer required to download.
                requested_files[i] = 0;

                // Write the output.
                write_output_file(rank, i, file_sizes[i], files[i]);

                // Check if all the requested files have completed.
                bool all_complete = true;

                for (int j = 0; j < MAX_FILES; j++) {
                    if (requested_files[j] != 0) {
                        all_complete = false;
                        break;
                    }
                }

                // Exit the download thread if all files have finished downloading.
                if (all_complete) {
                    
                    tracker_message_t tracker_message;
                    tracker_message.code = 8;

                    // Signal the tracker that this client has finished all downloads.
                    MPI_Ssend(&tracker_message, 1, create_tracker_message_datatype(), TRACKER_RANK, 8, MPI_COMM_WORLD);
                    return NULL;
                }
            }
        }
    }

    return NULL;
}


/**
 * Thread function that handles uploading segments to other peers.
*/
void *upload_thread_func(void *arg)
{
    // Unpack the arguments.
    upload_thread_arg_t* upload_arg = (upload_thread_arg_t*) arg;
    int rank = upload_arg->rank;
    int num_clients = upload_arg->num_clients;
    vector<vector<string>> files = upload_arg->files;
    vector<int> file_sizes = upload_arg->file_sizes;

    while (true) {

        MPI_Status s;
        peer_message_t peer_message;
        
        // Receive a message.
        MPI_Recv(&peer_message, 1, create_peer_message_datatype(), MPI_ANY_SOURCE, 4, MPI_COMM_WORLD, &s);

        int file_index = peer_message.file_index;
        int segment_index = peer_message.segment_index;
        int client_rank = s.MPI_SOURCE;

        if (file_index == -1 && segment_index == -1) {
            // Shutdown.
            return NULL;
        }

        // Get the requested hash.
        auto hash = files[file_index][segment_index].c_str();

        // Send it to the requesting client.
        MPI_Ssend(hash, HASH_SIZE, MPI_CHAR, client_rank, 5, MPI_COMM_WORLD);
    }

    return NULL;
}


void printSwarm(vector<vector<vector<int>>>& swarm, vector<int>&swarm_file_sizes, int num_clients) {
    cout << "Swarm file sizes:" << endl;
    for (int i = 0; i < MAX_FILES; i++) {
        cout << index_to_name(i) << ": " << swarm_file_sizes[i] << endl;
    }
    cout << endl;

    cout << "Swarm:" << endl;
    for (int i = 0; i < MAX_FILES; i++) {
        cout << index_to_name(i) << ":" << endl;

        for (int j = 0; j < num_clients; j++) {
            cout << "client " << j+1 << ":" << endl;

            for (int k = 0; k < MAX_CHUNKS; k++) {
                cout << swarm[i][j][k] << " ";
            }
            cout << endl;
        }
        cout << endl;
    }
}


/**
 * Function that handles requests sent to the tracker.
*/
void tracker(int numtasks, int rank) {

    int num_clients = numtasks - 1;

    // Swarm structure, swarm[i][j][k] : i = file index, j = client index, k = segment. 
    vector<vector<vector<int>>> swarm(MAX_FILES, vector<vector<int>>(num_clients, vector<int>(MAX_CHUNKS, 0)));

    // File sizes. swarm_file_sizes[i] : i = file index.
    vector<int> swarm_file_sizes(MAX_FILES, 0);

    MPI_Barrier(MPI_COMM_WORLD);

    // Receive which files each client has.
    for (int i = 0; i < num_clients; i++) {

        int client_file_sizes[MAX_FILES];
        memset(client_file_sizes, 0, sizeof(client_file_sizes));

        MPI_Status s;

        MPI_Recv(client_file_sizes, MAX_FILES, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &s);

        int client_rank = s.MPI_SOURCE;

        // In the swarm, for each file, mark all its chunks as being at client who sent the message.

        for (int j = 0; j < MAX_FILES; j++) {
            if (client_file_sizes[j] != 0) {
                
                swarm_file_sizes[j] = client_file_sizes[j];

                for (int k = 0; k < client_file_sizes[j]; k++) {
                    swarm[j][client_rank-1][k] = k+1;
                }
            }
        }
    }    

    // Send file sizes to all clients (instead of OK signal).
    for (int i = 0; i < num_clients; i++) {
        MPI_Ssend(&swarm_file_sizes[0], MAX_FILES, MPI_INT, i+1, 2, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Stores how many clients are currently downloading files.
    int num_downloading_clients = num_clients;

    while (true) {

        tracker_message_t tracker_message;
        MPI_Status s;
        
        // Receive a message.
        MPI_Recv(&tracker_message, 1, create_tracker_message_datatype(), MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &s);
        
        int tag = s.MPI_TAG;
        int client_rank = s.MPI_SOURCE;



        if (tag == 3) {
            
            // Tag 3: the client wants a list of all peers and their chunks for file at file_index.

            int file_index = tracker_message.file_index;

            // Build the reply.
            int peers[num_clients][MAX_CHUNKS];
            for (int i = 0; i < num_clients; i++) {
                for (int j = 0; j < MAX_CHUNKS; j++) {
                    peers[i][j] = swarm[file_index][i][j];
                }
            }

            MPI_Ssend(peers, num_clients * MAX_CHUNKS, MPI_INT, client_rank, 3, MPI_COMM_WORLD);
        }
        else if (tag == 6) {

            // Tag 6: Client has finished downloading file at file_index.

            int file_index = tracker_message.file_index;
            
            // Update the swarm to show that all chunks of file at file_index are available on this client.
            for (int i = 0; i < swarm_file_sizes[i]; i++) {
                swarm[file_index][client_rank-1][i] = i+1;
            }
        }
        else if (tag == 7) {
            
            // Tag 7: Client wants to update its swarm file list.

            int file_index = tracker_message.file_index;
            int segment_index = tracker_message.segment_index;

            swarm[file_index][client_rank-1][segment_index] = segment_index + 1;
        }
        else if (tag == 8) {

            // Tag 8: Client has finished downloading all files.

            // Decrement number of clients downloading files.
            num_downloading_clients--;

            if (num_downloading_clients == 0) {

                // All clients have finished downloading, signal all clients to close the upload thread.

                for (int i = 0; i < num_clients; i++) {
                    peer_message_t peer_message;
                    peer_message.file_index = -1;
                    peer_message.segment_index = -1;

                    MPI_Ssend(&peer_message, 1, create_peer_message_datatype(), i+1, 4, MPI_COMM_WORLD);
                }

                // Finally, exit the tracker.
                return;
            }
        }
    }
}


/**
 * Reads a peer's input file.
*/
auto read_input_file(int rank) {

    // Create the file name.
    string input_file = "./in" + to_string(rank) + ".txt";

    // Open the file.
    ifstream file(input_file);

    int num_files;
    int num_requested_files;
    vector<int> file_sizes(MAX_FILES, 0);
    vector<vector<string>> files(MAX_FILES, vector<string>(MAX_CHUNKS));
    vector<int> requested_files(MAX_FILES, 0);

    // Read the number of files this client has.
    file >> num_files;

    // Read segments for each file.
    for (int i = 0; i < num_files; i++) {
        string file_name;
        int num_segments;

        file >> file_name >> num_segments;
    
        vector<string> segments;

        for (int j = 0; j < num_segments; j++) {
            string segment;

            file >> segment;
            segments.push_back(segment);
        }

        file_sizes[name_to_index(file_name)] = num_segments;
        files[name_to_index(file_name)] = segments;
    }


    // Read the number of files the client wants to download.
    file >> num_requested_files;

    // Read the file names.
    for (int i = 0; i < num_requested_files; i++) {
        string requested_file;
        file >> requested_file;

        requested_files[name_to_index(requested_file)] = 1; 
    }

    file.close();

    return make_tuple(num_files, file_sizes, files, num_requested_files, requested_files);
}


/**
 * Function that handles initial peer logic.
*/
void peer(int numtasks, int rank) {

    // Read the input file.
    auto [num_files, file_sizes, files, num_requested_files, requested_files] = read_input_file(rank);

    // Get the files sizes from the tracker.
    int swarm_file_sizes[MAX_FILES];
    memset(swarm_file_sizes, 0, sizeof(swarm_file_sizes));

    MPI_Status s;

    MPI_Ssend(&file_sizes[0], MAX_FILES, MPI_INT, TRACKER_RANK, 1, MPI_COMM_WORLD);
    MPI_Recv(swarm_file_sizes, MAX_FILES, MPI_INT, TRACKER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &s);
    
    // Select only the file sizes for the requested files.
    for (int i = 0; i < MAX_FILES; i++) {
        if (requested_files[i] == 1) {
            file_sizes[i] = swarm_file_sizes[i];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Build thread arguments.
    auto download_thread_arg = new download_thread_arg_t;
    download_thread_arg->rank = rank;
    download_thread_arg->num_clients = numtasks - 1;
    download_thread_arg->requested_files = requested_files;
    download_thread_arg->file_sizes = file_sizes;

    auto upload_thread_arg = new upload_thread_arg_t;
    upload_thread_arg->rank = rank;
    upload_thread_arg->num_clients = numtasks - 1;
    upload_thread_arg->files = files;
    upload_thread_arg->file_sizes = file_sizes;

    // Start the threads.
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) download_thread_arg);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) upload_thread_arg);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
