/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*Will Adams and Nicholas Jackson
  CSCE 438 Section 500*/

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include "fb.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using hw2::Message;
using hw2::ListReply;
using hw2::Request;
using hw2::Reply;
using hw2::MessengerServer;
using hw2::ServerChat;
using hw2::Credentials;
using hw2::DataSync;
using grpc::Channel;
using grpc::ClientContext;
// Forwards ////////////////////////////////////////////////////////////////////
class ServerChatClient;

// Global Variables ////////////////////////////////////////////////////////////
bool isMaster = false;
bool isLeader = false;
std::string port = "2323"; // Port for clients to connect to
std::string workerPort = "8888"; // Port for workers to connect to
std::string workerToConnect = "8889"; // Port for this process to contact
std::string masterPort = "10001"; // Port that leading master monitors
std::vector<std::string> defaultWorkerPorts;
std::vector<std::string> defaultWorkerHostnames;
std::vector<ServerChatClient> localWorkersComs;
//std::vector<std::string> messageIDs; //messageIDs for the master server to have
static ServerChatClient* masterCom; // Connection to leading master
std::string host_x = "";
std::string host_y = "";
std::string masterHostname = "localhost"; // MUST BE LOCALHOST! (for masterCom in heartBeat)
//Vector that stores every client that has been created

static int server_id = 0;
// Utility Classes ////////////////////////////////////////////////////////////
//Client struct that holds a user's username, followers, and users they follow
struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
}; 

class vectorClock {
private:
	/* data */
	std::vector<google::protobuf::Timestamp> clock;
public:
	vectorClock (int unique_server_id, int vectorSize){
		// @TODO: Create vector
	}
	bool operator<(const google::protobuf::Timestamp &left){
		return false;
	}
	bool operator=(const google::protobuf::Timestamp &left){
		return false;
	}
	void updateClock(google::protobuf::Timestamp){
		
	}
	virtual ~vectorClock ();
};


//Vector that stores every client that has been created
std::vector<Client> client_db;
std::vector<std::string> masterList;

//Very rudimentary implementation of datasync
Request convertClient(Client client){
  Request request;
  std::string toStore;

  toStore = client.username + " ~~ " + "true " + std::to_string(client.following_file_size) + " ~~ " + "0";
  request.set_username(toStore);

  for(int i =0; i< client.client_followers.size(); i++){
    request.add_arguments(client.client_followers[i]->username);
  }
  request.add_arguments("~~");

  for(int i =0; i< client.client_following.size(); i++){
    request.add_arguments(client.client_following[i]->username);
  }

  return request;
}

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

int find_user_in_master(std::string username){
  int index = 0;
  for(std::string c : masterList){
    if(c == username)
      return index;
    index++;
  }
  return -1;
}

bool doesFileExist(std::string fileName){
    std::ifstream infile(fileName);
    return infile.good();
}

std::vector<std::string> collectIDs(){
    std::string line;
    std::vector<std::string> fileIDs;
    std::string delimiter = "::";
    unsigned int curLine = 0;

    for(int i = 0; i < client_db.size(); i++){
      std::string filename = client_db[i].username+".txt";
      if(doesFileExist(filename)){
        std::ifstream file(filename);
        while(std::getline(file, line)) {
          curLine++;
          std::string id = line.substr(0, line.find(delimiter));
          if (line.find(id, 0) != std::string::npos) {
            fileIDs.push_back(id);
          }
        }
      }
    } 
    return fileIDs;
}

std::string findMessageFromID(std::string id){
  std::string line = "";
  std::string delimiter = "::";
  unsigned int curLine = 0;

  for(int i = 0; i < client_db.size(); i++){
    std::string filename = client_db[i].username+".txt";
    if(doesFileExist(filename)){
      std::ifstream file(filename);
      while(std::getline(file, line)) {
        curLine++;
        if(id.compare(line.substr(0, line.find(delimiter))) == 0){
          return line;
        }
      }
    }
  } 
  return line;
}

// Recieving side of interserver chat Service
//MASTER SERVER PORTION
class ServerChatImpl final : public ServerChat::Service {
	// Asks for a reply and restarts the server if no replay is recieved
	Status pulseCheck(ServerContext* context, const Reply* in, Reply* out) override{
		std::string pid = std::to_string(getpid());
		out->set_msg(std::to_string(server_id));
    if(isMaster && isLeader){
      out->set_msg(std::to_string(server_id) + " (master)");
    }
		return Status::OK;
	}
	
	// gets the id of a connection
	Status idCheck(ServerContext* context, const Reply* in, Reply* out) override{
		std::string pid = std::to_string(getpid());
		out->set_msg(std::to_string(server_id));
		return Status::OK;
	}
//All workers send to master, master writes to its own database if the message isnt already there
  //Thats what datasend does
//When a worker requests a dataSync, worker sends IDs of all its message to the master
  //The master takes the set difference of all its own database message IDs, and IDs it received from the worker request
  //And then replies with the messages that it has, and receives the messages from the worker that it doesnt have
  //Could have a dataSync message type that has a repeatable string in it for sending lists of IDs and the message response list


/*
A clientWorker is going to call the serverChatClient version of DataSend every time they are about the write a message.
Datasend sends the information to the master for the master to write to its database.

A clientWorker calls dataSync before a client chats for the first time. Maybe called in a loop in CHAT.
NEEDS TO BE CALLED SOMEWHERE IN CHAT.
Only goes to the worker that requested it.
Put it at the start of the chatmessage. It would be good if it was also run
every X seconds.
*/

  //Add user data to master's database
  Status DataSend(ServerContext* context, const Reply* in, Reply* out) override{
    unsigned int curLine = 0;
    bool flag = false;
    std::string line;
    std::string toWrite = in->msg();
    std::string delimiter = "::";
    
    std::string id = toWrite.substr(0, toWrite.find(delimiter));
    toWrite.erase(0, toWrite.find(delimiter) + delimiter.length());
    toWrite.erase(0, toWrite.find(delimiter) + delimiter.length());
    
    std::string nameandmessage = toWrite.substr(0, toWrite.find(delimiter));
    std::string username = nameandmessage.substr(0, nameandmessage.find(':'));
    std::string filename = username+".txt";

    if(find_user(username) < 0){
      Client c;
      c.username = username; 
      client_db.push_back(c);
    }

    std::ifstream file(filename);
    while(std::getline(file, line)) {
      curLine++;
      if (line.find(id, 0) != std::string::npos) {
          flag = true;
      }
    }
    if(flag == true){
      return Status::OK;
    }
    else{
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      user_file << in->msg();
      return Status::OK;
    }
    return Status::OK;
  }

  //Add the following data to the master's database
  Status DataSendFollowers(ServerContext* context, const Request* in, Reply* out) override{
    unsigned int curLine = 0;
    bool flag = false;
    std::string line;
    std::string followerfile = in->username();
    std::string toWrite = in->arguments(0);
    std::string delimiter = "::";
    if(in->arguments(1).compare("true") == 0){
      followerfile = followerfile + "following.txt";
    }
    else
      followerfile = followerfile + ".txt";

    std::string id = toWrite.substr(0, toWrite.find(delimiter));

    std::ifstream file(followerfile);
    while(std::getline(file, line)) {
      curLine++;
      if (line.find(id, 0) != std::string::npos) {
          flag = true;
      }
    }
    if(flag == true){
      return Status::OK;
    }
    else{
      std::ofstream user_file(followerfile,std::ios::app|std::ios::out|std::ios::in);
      user_file << in->arguments(0);
      return Status::OK;
    }
    return Status::OK;
  }

  Status dataSync(ServerContext *context, const DataSync* in, DataSync* out) override{
    if(isMaster){
        std::vector<std::string> masterIDs = collectIDs();
        std::vector<std::string> workerIDs;
        std::vector<std::string> diffList;
        std::string found;

        for(int i=0; i< (in->ids().size()); i++){
          workerIDs.push_back(in->ids(i));
        }

        for (int i = 0; i < masterIDs.size(); i++) {
          for (int k = 0; k < workerIDs.size(); k++) {
            if (masterIDs[i] == workerIDs[k]) {
              found = ""; // add this
              break;
            } 
            else if (masterIDs[i] != workerIDs[k]) {
              found = masterIDs[i];
            }
          }
          if (found.compare("") == 1) { // to trigger this and not save garbage
            diffList.push_back(found);
          }
        }

        if(diffList.size() != 0){
          for(int i = 0; i < diffList.size(); i++){
            out->add_messages(findMessageFromID(diffList[i]));
          }
        }
        return Status::OK;
      }
     else
      return Status::CANCELLED;
    return Status::OK;
  }

  Status UpdateDatabase(ServerContext* context, const Request* in, Reply* out) override{
    if(isMaster){
      if(find_user_in_master(in->username()) < 0){
        masterList.push_back(in->username());
        int index;

        std:: string client_database = "client_database.txt";
        std::ofstream file(client_database,std::ios::app|std::ios::out|std::ios::in);

        //Really shitty hackey way of doing this but oh well:
        std::string input = in->username() + " followers:"; 
        for(int i=0; i< (in->arguments().size()); i++){
          if(in->arguments(i).compare("~~") == 0){
            input = input + " " + in->arguments(i);
          }
          else{
            index = i;
            input = input + " following:";
            break;
          }
        }
        for(int i=index; i < (in->arguments().size()); i++){
           input = input + " " + in->arguments(i);
        }
        file << input;
        return Status::OK;
      }
      else{
        int index;
        
        std::string input = in->username() + " followers:"; 
        for(int i=0; i< (in->arguments().size()); i++){
          if(in->arguments(i).compare("~~") == 0){
            input = input + " " + in->arguments(i);
          }
          else{
            index = i;
            input = input + " following:";
            break;
          }
        }
        for(int i=index; i < (in->arguments().size()); i++){
           input = input + " " + in->arguments(i);
        }
        std::string line;
        std::ifstream filein("client_database.txt");
        std::ofstream fileout("tempfile.txt", std::ios::app|std::ios::out|std::ios::in);
        while(std::getline(filein, line)) {
          if (line.find(in->username(), 0)) 
            line = input;
          else
            fileout << line;
        }
        rename("client_database.txt", "delete.txt");
        rename("tempfile.txt", "client_database.txt");
        remove("delete.txt");
      }
    }
    return Status::OK;
  }

};

// Sending side of interserver chat Service
//CLIENTWORKER SERVER PORTION
class ServerChatClient {
private:
	std::unique_ptr<ServerChat::Stub> stub_;
public:
 ServerChatClient(std::shared_ptr<Channel> channel)
	 : stub_(ServerChat::NewStub(channel)) {}

	 // Checks if other endpoint is responsive
	 bool pulseCheck() {
		 // Data we are sending to the server.
		 
		 Reply request;
		 request.set_msg("a");

		 // Container for the data we expect from the server.
		 Reply reply;

		 // Context for the client. It could be used to convey extra information to
		 // the server and/or tweak certain RPC behaviors.
		 ClientContext context;

		 // The actual RPC.
		 Status status = stub_->pulseCheck(&context, request, &reply);

		 // Act upon its status.
		 if (status.ok()) {
			 sleep(1);
			  std::cout << "Pulse on server id: " << server_id << " --> " << reply.msg() << std::endl;
			 return true;
		 } else {
			  std::cout << "No Pulse on server id: " << server_id << " --> " <<
				status.error_code() << ": " << status.error_message()
			 					 << std::endl;
			return false;
		 }
	 }

	 // Checks if other endpoint is responsive
	 bool idCheck() {
		 // Data we are sending to the server.
		 Reply request;
		 request.set_msg("-1"); //error

		 // Container for the data we expect from the server.
		 Reply reply;

		 // Context for the client. It could be used to convey extra information to
		 // the server and/or tweak certain RPC behaviors.
		 ClientContext context;

		 // The actual RPC.
		 Status status = stub_->idCheck(&context, request, &reply);

		 // Act upon its status.
		 if (status.ok()) {
				//std::cout << "Pulse " << workerPort << " --> " << reply.msg() << std::endl;
			 return true;
		 } else {
				 //std::cout << "Why didn't Nick Implement this the first time through";
				std::cout << status.error_code() << ": " << status.error_message()
								 << std::endl;
			return false;
		 }
	 }

    //Forwards messages to the master when it receives a message
    void DataSend(std::string input){
      Reply request;
      Reply reply;
      request.set_msg(input);
      ClientContext context;

      Status status = stub_->DataSend(&context, request, &reply);
      if(status.ok()){
        std::cout<<"Database synchronized.";
      } else{
        std::cout << status.error_code() << ": " << status.error_message()
                 << std::endl;
      }
    }

    void DataSendFollowers(std::string followerfile, std::string input, bool isFollowerFile){
      Request request;
      Reply reply;
      request.set_username(followerfile);
      request.add_arguments(input);
      if(isFollowerFile){
        request.add_arguments("true");
      }
      else
        request.add_arguments("false");
      
      ClientContext context;

      Status status = stub_->DataSendFollowers(&context, request, &reply);
      if(status.ok()){
        std::cout<<"Database synchronized.";
      } else{
        std::cout << status.error_code() << ": " << status.error_message()
                 << std::endl;
      }
    }

    //TODO
    void dataSync(std::vector<std::string> allMessageIDs){
      DataSync clientIDs;
      DataSync reply;
      ClientContext context;
      std::string delimiter = "::";

      for(int i=0; i<allMessageIDs.size(); i++){
        clientIDs.add_ids(allMessageIDs[i]);
      }

      Status status = stub_->dataSync(&context, clientIDs, &reply);

      if(status.ok()){
        std::string delimiter = "::";
        for(int i = 0; i < reply.messages().size(); i++){
          std::string message = reply.messages(i);
          std::string modified = message;
          modified.erase(0, modified.find(delimiter) + delimiter.length());
          modified.erase(0, modified.find(delimiter) + delimiter.length());
          std::string nameandmessage = modified.substr(0, modified.find(delimiter));
          std::string username = nameandmessage.substr(0, nameandmessage.find(':'));
          std::string filename = username+".txt";
          std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
          user_file << message;
        }
        std::cout<<"Database synchronized.";
      } else{
        std::cout << status.error_code() << ": " << status.error_message()
                 << std::endl;
      }
    }

    void UpdateDatabase(Request request){
      Reply reply;
      ClientContext context;
      Status status = stub_->UpdateDatabase(&context, request, &reply);
      if(status.ok()){
        std::cout<<"Database synchronized.";
      } else{
        std::cout << status.error_code() << ": " << status.error_message()
                 << std::endl;
      }
    }
};

// Logic and data behind the server-client behavior.
class MessengerServiceImpl final : public MessengerServer::Service {
  
  //Sends the list of total rooms and joined rooms to the client
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    // int index = 0; // Possible uneeded
    for(Client c : client_db){
      list_reply->add_all_rooms(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_following.begin(); it!=user.client_following.end(); it++){
      list_reply->add_joined_rooms((*it)->username);
    }
    return Status::OK;
  }

  //Sets user1 as following user2
  Status Join(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int join_index = find_user(username2);
    //If you try to join a non-existent client or yourself, send failure message
    if(join_index < 0 || username1 == username2)
      reply->set_msg("Join Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      //If user1 is following user2, send failure message
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	reply->set_msg("Join Failed -- Already Following User");
        return Status::OK;
      }
      user1->client_following.push_back(user2);
      user2->client_followers.push_back(user1);

      masterCom->UpdateDatabase(convertClient(*user1));
      masterCom->UpdateDatabase(convertClient(*user2));
      reply->set_msg("Join Successful");
    }
    return Status::OK; 
  }

  //Sets user1 as no longer following user2
  Status Leave(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    //If you try to leave a non-existent client or yourself, send failure message
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("Leave Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      //If user1 isn't following user2, send failure message
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("Leave Failed -- Not Following User");
        return Status::OK;
      }
      // find the user2 in user1 following and remove
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      // find the user1 in user2 followers and remove
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      masterCom->UpdateDatabase(convertClient(*user1));
      masterCom->UpdateDatabase(convertClient(*user2));
      reply->set_msg("Leave Successful");
    }
    return Status::OK;
  }

  //Called when the client startd and checks whether their username is taken or not
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    Client c;
    std::string username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);

      masterCom->UpdateDatabase(convertClient(c));
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	reply->set_msg(msg);
        user->connected = true;
      }
    }
    return Status::OK;
  }

  Status Chat(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    Client *c;

    //Read messages until the client disconnects
    while(stream->Read(&message)) {
      std::string username = message.username();
      std::string unique_id = message.id();  
      int user_index = find_user(username);
      c = &client_db[user_index];
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = unique_id +" :: "+time+" :: "+message.username()+":"+message.msg()+"\n";
 //     messageIDs.push_back(unique_id);
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream"){
        user_file << fileinput;
        masterCom->DataSend(fileinput);
      }
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream == 0)
      	  c->stream = stream;

        //CALL FOR DATASYNC(messageIDs)
        masterCom->dataSync(collectIDs());

        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
	for(uint i = 0; i<newest_twenty.size(); i++){
	  new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected)
	  temp_client->stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
  masterCom->DataSendFollowers(temp_username, fileinput, true);
        temp_client->following_file_size++;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
        masterCom->DataSendFollowers(temp_username, fileinput, false);
      }
    }
    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }

  //Sends the client to the master server, then sends the client to the first available worker. 
  //Needs to be re-evaluated to actually connect client to new server
  //But works for now, not finished
  Status SendCredentials(ServerContext* context, const Credentials* credentials, Credentials* reply) override {
    std::cout<<"In SendCredentials Serverside"<< std::endl;
    std::string hostname = credentials->hostname();
    std::string portnumber = credentials->portnumber();

    if(!isMaster){
      std::cout << "Redirecting client to Leader: " << masterPort << std::endl;
      reply->set_hostname(masterHostname);
      reply->set_portnumber("2323");
      reply->set_confirmation("toMaster");
    }
    else if (isMaster || isLeader){
      std::cout << "Redirecting client to Worker: " << defaultWorkerHostnames[0] << std::endl;
      reply->set_hostname(defaultWorkerHostnames[0]);
      reply->set_portnumber("2323");
      reply->set_confirmation("toWorker");
    } //It will write data to all servers, however it will only chat between users connected to the same server
    else{
      std::cout<<"There's been an error in the redirect: SendCredentials.'\n'";
      return Status::CANCELLED;
    }
    return Status::OK;
  }

};

// Starts a new server process on the same worker port as the crashed process
void* startNewServer(void* missingPort){
	int* mwp = (int*) missingPort;
	std::string missingWorkerPort = std::to_string(*mwp);
	std::string cmd = "./fbsd";
	std::string id = "";
	int id_rank = 0; // <3 = masters, <6 = server 1, >5 = server 2
	// Sad Hack to figure out id
	if(server_id < 3){
		// master
		id_rank = 0;
		if (missingWorkerPort == "10001") {
			// keep id
		} else if (missingWorkerPort == "10002") {
			id_rank = id_rank + 1;
		} else if (missingWorkerPort == "10003")  {
			id_rank = id_rank + 2;
		} else {
			std::cerr << "Error: Couldn't determine crashed process id" << '\n';
		}
	}
	else if (server_id > 5){
		// server 2
		id_rank = 6;
		if (missingWorkerPort == "10001") {
			// keep id
		} else if (missingWorkerPort == "10002") {
			id_rank = id_rank + 1;
		} else if (missingWorkerPort == "10003")  {
			id_rank = id_rank + 2;
		} else {
			std::cerr << "Error: Couldn't determine crashed process id" << '\n';
		}
	}
	else{
		// server 1
		id_rank = 3;
		if (missingWorkerPort == "10001") {
			// keep id
		} else if (missingWorkerPort == "10002") {
			id_rank = id_rank + 1;
		} else if (missingWorkerPort == "10003")  {
			id_rank = id_rank + 2;
		} else {
			std::cerr << "Error: Couldn't determine crashed process id" << '\n';
		}
	}
	
	id = std::to_string(id_rank);
	// If the leader falls, create a new leading master
	if ((missingWorkerPort == masterPort) && isMaster){
		std::cout <<server_id << ": " << "Fixing master crash on port: " << missingWorkerPort << '\n';
		cmd = cmd + " -p " + port; //port is a global varaible
		cmd = cmd + " -x " + host_x;
		cmd = cmd + " -y " + host_y;
		cmd = cmd + " -m";
		cmd = cmd + " -l";
		cmd = cmd + " -w " + missingWorkerPort;
		cmd = cmd + " -i " + id;
	}
	// Create a non-leader master
	else if (isMaster){
		std::cout <<server_id << ": " << "Fixing master crash on port: " << missingWorkerPort << '\n';
		cmd = cmd + " -p " + port; //port is a global varaible
		cmd = cmd + " -x " + host_x;
		cmd = cmd + " -y " + host_y;
		cmd = cmd + " -m";
		cmd = cmd + " -w " + missingWorkerPort;
		cmd = cmd + " -i " + id;
	}
	else{
		std::cout <<server_id << ": " << "Fixing worker crash on port: " << missingWorkerPort << '\n';
		cmd = cmd + " -p " + port; //port is a global varaible
		cmd = cmd + " -x " + host_x;
		cmd = cmd + " -r " + masterHostname; // masterPort hardcoded (4/19/17 1:54pm)
		cmd = cmd + " -w " + missingWorkerPort;
		cmd = cmd + " -i " + id;
	}
	// THIS IS A BLOCKING FUNCTION!!!!!
	if(system(cmd.c_str()) == -1){
		std::cerr << "Error: Could not start new process" << '\n';
	}
	return 0;
}

// Get an exclusive lock on filename or return -1 (already locked)
//write a new function called electionLock that has none of the same code as the file lock
//inside of the electionLock, figure out if you have the highest ID on the machine
//If I had the highest ID, return true, if not, false
//Down in the heartbeat, start a new server thread inside of an if(electionlock) that way only the person who wins the
//election does it
//
bool electionLock(){
  int maxid = -1;
  //if id > maxid, maxid = id

  for (size_t i = 0; i < localWorkersComs.size(); i++) {
    int temp = localWorkersComs[i].idCheck();
    if (server_id > temp){
      isLeader = true;
    }
    else{
      isLeader = false;
    }
  }
  if(isLeader)
    std::cout<<server_id + " Is Leader.";
  return isLeader;
}


// Get an exclusive lock on filename or return -1 (already locked)
int fileLock(std::string filename){
	// Create file if needed; open otherwise
	int fd = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
	if (fd < 0){
		std::cout << "Couldn't lock to restart process" << std::endl;
		return -1;
	}

	// -1 = File already locked; 0 = file unlocked
	int rc = flock(fd, LOCK_EX | LOCK_NB);
	return rc;
}

// Opposite of fileLock
int fileUnlock(std::string filename){
	// Create file if needed; open otherwise
	int fd = open(filename.c_str(), O_RDWR, 0666);
	if (fd < 0){
		std::cout << "Couldn't unlock file" << std::endl;
	}

	// -1 = Error; 0 = file unlocked
	int rc = flock(fd, LOCK_UN | LOCK_NB);
	return rc;
}

// Monitors and restarts other local prcesses if they crash
void* heartBeatMonitor(void* invalidMemory){
	pthread_t startNewServer_tid = -1;
	bool wasDisconnected = false;
	while(true){
		
		
		//@TODO: What if reonnecting just works and I need to focus on restarting?
		bool pulse = masterCom->pulseCheck();
		if(!pulse){
			std::cout << server_id << ": Reconnecting to master..." << '\n';
			sleep(1);
			masterCom = new ServerChatClient(grpc::CreateChannel(
				masterHostname + ":" + masterPort, grpc::InsecureChannelCredentials()));
			if (masterCom->pulseCheck()) {
				std::cout << "Reconnected: " << server_id <<"--> Master" << '\n';
			} else {
			std::cout << server_id << ": Failed to reconnect" << '\n';
			}
		}
		for (size_t i = 0; i < localWorkersComs.size(); i++) {
			std::string possiblyDeadPort = defaultWorkerPorts[i];
			int pdp = atoi(possiblyDeadPort.c_str());
			std::string contactInfo = "localhost:"+ possiblyDeadPort;
			
			if(localWorkersComs[i].pulseCheck()){
				// Connection alive
				if(wasDisconnected){
					std::cout << "Reconnected: " << workerPort << " --> " << possiblyDeadPort << '\n';
					// @TODO: 
					std::cout << "Start new election here" << '\n';
				}
				wasDisconnected = false;
			}
			else{
				wasDisconnected = true;
				// Connection dead
				if(electionLock()){
					// Start new process if file was unlocked
					pthread_create(&startNewServer_tid, NULL, &startNewServer, (void*) &pdp);
					
					// if(fileUnlock("heartBeatMonitorLock") == -1){
					// 	std::cerr << "Error unlocking heartBeatMonitorLock file" << '\n';
					// }

					std::cout << "Peer: " << workerPort << " reconnecting..." << '\n';
					sleep(1); 
					//  update connection info reguardless of who restarted  it
					localWorkersComs[i] = ServerChatClient(grpc::CreateChannel(
					contactInfo, grpc::InsecureChannelCredentials()));
				}
				else{
					std::cout << "Peer: " << workerPort << " reconnecting..." << '\n';
					sleep(1);
					//  update connection info reguardless of who restarted  it
					localWorkersComs[i] = ServerChatClient(grpc::CreateChannel(
					contactInfo, grpc::InsecureChannelCredentials()));
				}
			}
		}
	}
	return 0;
}

// Secondary service (for fault tolerence) to listen for connecting workers
void* RunServerCom(void* invalidMemory) {
	std::string server_address = "0.0.0.0:"+workerPort;
  ServerChatImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening for works on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
	return 0;
}

void setComLinks(){
	std::string contactInfo = "";
	if (defaultWorkerPorts.size() == 0){
		std::cout << "Error: Default ports uninitialized" << '\n';
	}
	for (size_t i = 0; i < defaultWorkerPorts.size(); i++) {
		if (defaultWorkerPorts[i] == workerPort){
			// Don't connect to self
		}
		else{
			contactInfo = "localhost:"+defaultWorkerPorts[i];
			std::cout << "Connecting: " << contactInfo << '\n';
			localWorkersComs.push_back(
				ServerChatClient(grpc::CreateChannel(
		  	contactInfo, grpc::InsecureChannelCredentials())));
		}
	}
	// Create connection to master host on leading master port 
	masterCom = new ServerChatClient(grpc::CreateChannel(
	masterHostname+":"+masterPort, grpc::InsecureChannelCredentials()));
}

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  MessengerServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening for clients on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  // Initialize default values
	defaultWorkerPorts.push_back("10001");
	defaultWorkerPorts.push_back("10002");
	defaultWorkerPorts.push_back("10003");
  defaultWorkerHostnames.push_back("lenss-comp4");
  defaultWorkerHostnames.push_back("lenss-comp1");
  defaultWorkerHostnames.push_back("lenss-comp3");

	// Parses options that start with '-' and adding ':' makes it mandontory
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:w:p:x:y:r:mli:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
					break;
			case 'x':
					host_x = optarg;
					break;
			case 'y':
					host_y = optarg;
					break;
			case 'r':
					masterHostname = optarg;
					break;
			case 'l':
					isLeader = true;
					break;
			case 'm':
					isMaster = true;
					break;
			case 'w':
					workerPort = optarg;
					break;
			case 'c':
					workerToConnect = optarg;
					break;
			case 'i':
					server_id = atoi(optarg);
					break;
	      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }

	pthread_t thread_id, heartbeat_tid = -1;
	sleep(1);
	pthread_create(&thread_id, NULL, &RunServerCom, (void*) NULL);
	sleep(1);
	// Set up communication links between worker servers
	setComLinks();
	sleep(1);
	// Monitor other local server heartbeats
	pthread_create(&heartbeat_tid, NULL, &heartBeatMonitor, (void*) NULL);	
	sleep(1);
	// Start servering clients
  RunServer(port);

  return 0;
}
