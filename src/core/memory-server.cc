//  Copyright © 2014 Anh Dinh. All Rights Reserved.

#include "core/memory-server.h"
#include "core/table-registry.h"
#include "utils/global_context.h"


namespace lapis{
	void MemoryServer::StartMemoryServer(){
		net_ = NetworkThread::Get();
		server_id_ = net_->id();
		manager_id_ = net_->size()-1;

		//  set itself as the current worker for the table
		TableRegistry::Map &t = TableRegistry::Get()->tables();
		for (TableRegistry::Map::iterator i = t.begin(); i != t.end(); ++i) {
		    i->second->set_worker(this);
		}

		//  register to the manager
		 RegisterWorkerRequest req;
		 req.set_id(server_id_);
		 net_->Send(manager_id_, MTYPE_REGISTER_WORKER, req);


		// register callbacks
		net_->RegisterCallback(MTYPE_SHARD_ASSIGNMENT,
		                                        boost::bind(&MemoryServer::HandleShardAssignment, this));

		net_->RegisterRequestHandler(MTYPE_PUT_REQUEST,
		                                        boost::bind(&MemoryServer::HandleUpdateRequest, this, _1));
		net_->RegisterRequestHandler(MTYPE_GET_REQUEST,
												boost::bind(&MemoryServer::HandleGetRequest, this, _1));

	}

	void MemoryServer::ShutdownMemoryServer(){
		net_->Flush();
		net_->Send(manager_id_, MTYPE_WORKER_END, EmptyMessage());
		EmptyMessage msg;
		int src=0;
		net_->Read(manager_id_, MTYPE_WORKER_SHUTDOWN, &msg, &src);
		net_->Shutdown();
	}

	void MemoryServer::HandleShardAssignment(){
		CHECK(GlobalContext::Get()->IsRoleOf(kMemoryServer, id())) << "Assign table to wrong server " << id();

		ShardAssignmentRequest shard_req;
		net_->Read(manager_id_, MTYPE_SHARD_ASSIGNMENT, &shard_req);
		//  request read from DistributedMemoryManager

		for (int i = 0; i < shard_req.assign_size(); i++) {
			const ShardAssignment &a = shard_req.assign(i);
			GlobalTable *t = TableRegistry::Get()->table(a.table());
			t->get_partition_info(a.shard())->owner = a.new_worker();

			EmptyMessage empty;
			net_->Send(manager_id_, MTYPE_SHARD_ASSIGNMENT_DONE, empty);

			LOG(INFO) << StringPrintf("Process %d is assigned shard (%d,%d)", NetworkThread::Get()->id(), a.table(), a.shard());
		}
	}

	//  respond to request
	void MemoryServer::HandleGetRequest(const Message* message){
		const HashGet* get_req = static_cast<const HashGet*>(message);
		TableData get_resp;

		// prepare
		get_resp.Clear();
		get_resp.set_source(server_id_);
		get_resp.set_table(get_req->table());
		get_resp.set_shard(-1);
		get_resp.set_done(true);
		get_resp.set_key(get_req->key());

		// fill data
		{
			GlobalTable * t = TableRegistry::Get()->table(get_req->table());
			t->handle_get(*get_req, &get_resp);
		}

		net_->Send(get_req->source(), MTYPE_GET_RESPONSE, get_resp);
	}

	void MemoryServer::HandleUpdateRequest(const Message* message){
		boost::recursive_mutex::scoped_lock sl(state_lock_);

		const TableData* put = static_cast<const TableData*>(message);

		GlobalTable *t = TableRegistry::Get()->table(put->table());
		t->ApplyUpdates(*put);
		int key = 0;
		key = *reinterpret_cast<const int*>((new StringPiece(put->key()))->data);
	}

	int MemoryServer::peer_for_partition(int table, int shard){
		return TableRegistry::Get()->tables()[table]->owner(shard);
	}
}
