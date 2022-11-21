// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#include "rgw_sal_s3.h"

#define dout_subsys ceph_subsys_rgw
#define dout_context g_ceph_context

namespace rgw { namespace sal {

static inline Bucket* nextBucket(Bucket* t)
{
  if (!t)
    return nullptr;

  return dynamic_cast<FilterBucket*>(t)->get_next();
}

static inline Object* nextObject(Object* t)
{
  if (!t)
    return nullptr;
  
  return dynamic_cast<FilterObject*>(t)->get_next();
}  

int S3FilterStore::initialize(CephContext *cct, const DoutPrefixProvider *dpp)
{
  FilterStore::initialize(cct, dpp);
  // FIXME - AMIN
  //blk_dir->init(cct);
  //d4n_cache->init(cct);
  
  return 0;
}

std::unique_ptr<User> S3FilterStore::get_user(const rgw_user &u)
{
  std::unique_ptr<User> user = next->get_user(u);

  return std::make_unique<S3FilterUser>(std::move(user), this);
}


std::unique_ptr<Object> S3FilterBucket::get_object(const rgw_obj_key& k)
{
  std::unique_ptr<Object> o = next->get_object(k);

  //FIXME
  //return std::make_unique<S3FilterObject>(std::move(o), this, filter);
  return NULL;
}


int S3FilterUser::create_bucket(const DoutPrefixProvider* dpp,
                              const rgw_bucket& b,
                              const std::string& zonegroup_id,
                              rgw_placement_rule& placement_rule,
                              std::string& swift_ver_location,
                              const RGWQuotaInfo * pquota_info,
                              const RGWAccessControlPolicy& policy,
                              Attrs& attrs,
                              RGWBucketInfo& info,
                              obj_version& ep_objv,
                              bool exclusive,
                              bool obj_lock_enabled,
                              bool* existed,
                              req_info& req_info,
                              std::unique_ptr<Bucket>* bucket_out,
                              optional_yield y)
{
  std::unique_ptr<Bucket> nb;
  int ret;

  ldpp_dout(dpp, 20) << "AMIN S3 Filter: Creating Bucket." << dendl;

  ret = next->create_bucket(dpp, b, zonegroup_id, placement_rule, swift_ver_location, pquota_info, policy, attrs, info, ep_objv, exclusive, obj_lock_enabled, existed, req_info, &nb, y);
  if (ret < 0)
    return ret;

  Bucket* fb = new S3FilterBucket(std::move(nb), this, filter);
  bucket_out->reset(fb);
  return 0;
}

int S3FilterStore::get_bucket(const DoutPrefixProvider* dpp, User* u, const rgw_bucket& b, std::unique_ptr<Bucket>* bucket, optional_yield y)
{
  std::unique_ptr<Bucket> nb;
  int ret;
  User* nu = nextUser(u);

  ret = next->get_bucket(dpp, nu, b, &nb, y);
  if (ret != 0)
    return ret;

  Bucket* fb = new S3FilterBucket(std::move(nb), u);
  bucket->reset(fb);
  return 0;
}

int S3FilterStore::get_bucket(User* u, const RGWBucketInfo& i, std::unique_ptr<Bucket>* bucket)
{
  std::unique_ptr<Bucket> nb;
  int ret;
  User* nu = nextUser(u);

  ret = next->get_bucket(nu, i, &nb);
  if (ret != 0)
    return ret;

  Bucket* fb = new S3FilterBucket(std::move(nb), u);
  bucket->reset(fb);
  return 0;
}

int S3FilterStore::get_bucket(const DoutPrefixProvider* dpp, User* u, const std::string& tenant, const std::string& name, std::unique_ptr<Bucket>* bucket, optional_yield y)
{
  std::unique_ptr<Bucket> nb;
  int ret;
  User* nu = nextUser(u);

  ret = next->get_bucket(dpp, nu, tenant, name, &nb, y);
  if (ret != 0)
    return ret;

  Bucket* fb = new S3FilterBucket(std::move(nb), u);
  bucket->reset(fb);
  return 0;
}




/*
int D4NFilterObject::set_obj_attrs(const DoutPrefixProvider* dpp, Attrs* setattrs,
                            Attrs* delattrs, optional_yield y) 
{
  if (setattrs != NULL) {
    if (delattrs != NULL) {
      for (const auto& attr : *delattrs) {
        if (std::find(setattrs->begin(), setattrs->end(), attr) != setattrs->end()) {
          delattrs->erase(std::find(delattrs->begin(), delattrs->end(), attr));
        }
      }
    }

    int updateAttrsReturn = filter->get_d4n_cache()->setObject(this->get_name(), &(this->get_attrs()), setattrs);

    if (updateAttrsReturn < 0) {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache set object attributes operation failed." << dendl;
    } else {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache set object attributes operation succeeded." << dendl;
    }
  }

  if (delattrs != NULL) {
    std::vector<std::string> delFields;
    Attrs::iterator attrs;

    // Extract fields from delattrs
    for (attrs = delattrs->begin(); attrs != delattrs->end(); ++attrs) {
      delFields.push_back(attrs->first);
    }

    Attrs currentattrs = this->get_attrs();
    std::vector<std::string> currentFields;
    
    // Extract fields from current attrs 
    for (attrs = currentattrs.begin(); attrs != currentattrs.end(); ++attrs) {
      currentFields.push_back(attrs->first);
    }
    
    int delAttrsReturn = filter->get_d4n_cache()->delAttrs(this->get_name(), currentFields, delFields);

    if (delAttrsReturn < 0) {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache delete object attributes operation failed." << dendl;
    } else {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache delete object attributes operation succeeded." << dendl;
    }
  }

  return next->set_obj_attrs(dpp, setattrs, delattrs, y);  
}

int D4NFilterObject::get_obj_attrs(optional_yield y, const DoutPrefixProvider* dpp,
                                rgw_obj* target_obj)
{
  rgw::sal::Attrs newAttrs;
  int getAttrsReturn = filter->get_d4n_cache()->getObject(this->get_name(), &(this->get_attrs()), &newAttrs);

  if (getAttrsReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache get object attributes operation failed." << dendl;

    return next->get_obj_attrs(y, dpp, target_obj);
  } else {
    int setAttrsReturn = this->set_attrs(newAttrs);
    
    if (setAttrsReturn < 0) {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache get object attributes operation failed." << dendl;

      return next->get_obj_attrs(y, dpp, target_obj);
    } else {
      ldpp_dout(dpp, 20) << "D4N Filter: Cache get object attributes operation succeeded." << dendl;
  
      return 0;
    }
  }
}

int D4NFilterObject::modify_obj_attrs(const char* attr_name, bufferlist& attr_val,
                               optional_yield y, const DoutPrefixProvider* dpp) 
{
  Attrs update;
  update[(std::string)attr_name] = attr_val;
  int updateAttrsReturn = filter->get_d4n_cache()->setObject(this->get_name(), &(this->get_attrs()), &update);

  if (updateAttrsReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache modify object attribute operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache modify object attribute operation succeeded." << dendl;
  }

  return next->modify_obj_attrs(attr_name, attr_val, y, dpp);  
}

int D4NFilterObject::delete_obj_attrs(const DoutPrefixProvider* dpp, const char* attr_name,
                               optional_yield y) 
{
  std::vector<std::string> delFields;
  delFields.push_back((std::string)attr_name);
  
  Attrs::iterator attrs;
  Attrs currentattrs = this->get_attrs();
  std::vector<std::string> currentFields;
  
  // Extract fields from current attrs 
  for (attrs = currentattrs.begin(); attrs != currentattrs.end(); ++attrs) {
    currentFields.push_back(attrs->first);
  }
  
  int delAttrReturn = filter->get_d4n_cache()->delAttrs(this->get_name(), currentFields, delFields);

  if (delAttrReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache delete object attribute operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache delete object attribute operation succeeded." << dendl;
  }
  
  return next->delete_obj_attrs(dpp, attr_name, y);  
}

std::unique_ptr<Object> D4NFilterStore::get_object(const rgw_obj_key& k)
{
  std::unique_ptr<Object> o = next->get_object(k);

  return std::make_unique<D4NFilterObject>(std::move(o), this);
}

std::unique_ptr<Writer> D4NFilterStore::get_atomic_writer(const DoutPrefixProvider *dpp,
				  optional_yield y,
				  std::unique_ptr<rgw::sal::Object> _head_obj,
				  const rgw_user& owner,
				  const rgw_placement_rule *ptail_placement_rule,
				  uint64_t olh_epoch,
				  const std::string& unique_tag)
{
  std::unique_ptr<Object> no = nextObject(_head_obj.get())->clone();

  std::unique_ptr<Writer> writer = next->get_atomic_writer(dpp, y, std::move(no),
							   owner, ptail_placement_rule,
							   olh_epoch, unique_tag);

  return std::make_unique<D4NFilterWriter>(std::move(writer), this, std::move(_head_obj), dpp, true);
}

std::unique_ptr<Object::ReadOp> D4NFilterObject::get_read_op()
{
  std::unique_ptr<ReadOp> r = next->get_read_op();
  return std::make_unique<D4NFilterReadOp>(std::move(r), this);
}

std::unique_ptr<Object::DeleteOp> D4NFilterObject::get_delete_op()
{
  std::unique_ptr<DeleteOp> d = next->get_delete_op();
  return std::make_unique<D4NFilterDeleteOp>(std::move(d), this);
}

int D4NFilterObject::D4NFilterReadOp::prepare(optional_yield y, const DoutPrefixProvider* dpp)
{
  int getDirReturn = source->filter->get_block_dir()->getValue(source->filter->get_cache_block());

  if (getDirReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Directory get operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Directory get operation succeeded." << dendl;
  }

  rgw::sal::Attrs newAttrs;
  source->filter->get_d4n_cache()->getObject(source->get_name(), &(source->get_attrs()), &newAttrs);
  int getObjReturn = source->set_attrs(newAttrs);

  if (getObjReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache get operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache get operation succeeded." << dendl;
  }
  
  return next->prepare(y, dpp);
}

int D4NFilterObject::D4NFilterDeleteOp::delete_obj(const DoutPrefixProvider* dpp,
					   optional_yield y)
{
  int delDirReturn = source->filter->get_block_dir()->delValue(source->filter->get_cache_block());

  if (delDirReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Directory delete operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Directory delete operation succeeded." << dendl;
  }

  int delObjReturn = source->filter->get_d4n_cache()->delObject(source->get_name());

  if (delObjReturn < 0) {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache delete operation failed." << dendl;
  } else {
    ldpp_dout(dpp, 20) << "D4N Filter: Cache delete operation succeeded." << dendl;
  }

  return next->delete_obj(dpp, y);
}

int D4NFilterWriter::prepare(optional_yield y) 
{
  int delDataReturn = filter->get_d4n_cache()->deleteData(head_obj->get_name()); 

  if (delDataReturn < 0) {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache delete data operation failed." << dendl;
  } else {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache delete data operation succeeded." << dendl;
  }

  return next->prepare(y);
}

int D4NFilterWriter::process(bufferlist&& data, uint64_t offset)
{
  bufferlist objectData = data;
  int appendDataReturn = filter->get_d4n_cache()->appendData(head_obj->get_name(), data);

  if (appendDataReturn < 0) {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache append data operation failed." << dendl;
  } else {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache append data operation succeeded." << dendl;
  }

  return next->process(std::move(data), offset);
}

int D4NFilterWriter::complete(size_t accounted_size, const std::string& etag,
                       ceph::real_time *mtime, ceph::real_time set_mtime,
                       std::map<std::string, bufferlist>& attrs,
                       ceph::real_time delete_at,
                       const char *if_match, const char *if_nomatch,
                       const std::string *user_data,
                       rgw_zone_set *zones_trace, bool *canceled,
                       optional_yield y)
{
  cache_block* temp_cache_block = filter->get_cache_block();
  RGWBlockDirectory* temp_block_dir = filter->get_block_dir();

  temp_cache_block->hosts_list.push_back(temp_block_dir->get_host() + ":" + std::to_string(temp_block_dir->get_port())); 
  temp_cache_block->size_in_bytes = accounted_size;
  temp_cache_block->c_obj.bucket_name = head_obj->get_bucket()->get_name();
  temp_cache_block->c_obj.obj_name = head_obj->get_name();

  int setDirReturn = temp_block_dir->setValue(temp_cache_block);

  if (setDirReturn < 0) {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Directory set operation failed." << dendl;
  } else {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Directory set operation succeeded." << dendl;
  }
   
  // Remove later -Sam
  int ret = next->complete(accounted_size, etag, mtime, set_mtime, attrs,
			delete_at, if_match, if_nomatch, user_data, zones_trace,
			canceled, y);
  head_obj->get_obj_attrs(y, save_dpp, NULL);

  // Append additional metadata to attributes
  rgw::sal::Attrs newAttrs = head_obj->get_attrs();
  rgw::sal::Attrs attrs_temp = head_obj->get_attrs();
  buffer::list bl;
  RGWObjState* astate;
  head_obj->get_obj_state(save_dpp, &astate, y);

  bl.append(to_iso_8601(head_obj->get_mtime()));
  newAttrs.insert({"mtime", bl});
  bl.clear();

  bl.append(std::to_string(head_obj->get_obj_size()));
  newAttrs.insert({"object_size", bl});
  bl.clear();

  bl.append(std::to_string(accounted_size));
  newAttrs.insert({"accounted_size", bl});
  bl.clear();
 
  bl.append(std::to_string(astate->accounted_size));
  newAttrs.insert({"cur_accounted_size", bl});
  bl.clear();
 
  bl.append(std::to_string(astate->epoch));
  newAttrs.insert({"epoch", bl});
  bl.clear();

  if (head_obj->have_instance()) {
    bl.append(head_obj->get_instance());
    newAttrs.insert({"version_id", bl});
    bl.clear();
  }

  auto iter = attrs_temp.find(RGW_ATTR_SOURCE_ZONE);
  if (iter != attrs_temp.end()) {
    bl.append(std::to_string(astate->zone_short_id));
    newAttrs.insert({"source_zone_short_id", bl});
    bl.clear();
  }

  bl.append(std::to_string(head_obj->get_bucket()->get_count()));
  newAttrs.insert({"bucket_count", bl});
  bl.clear();

  bl.append(std::to_string(head_obj->get_bucket()->get_size()));
  newAttrs.insert({"bucket_size", bl});
  bl.clear();

  RGWQuota quota;
  filter->get_quota(quota);
  bl.append(std::to_string(quota.user_quota.max_size));
  newAttrs.insert({"user_quota.max_size", bl});
  bl.clear();

  bl.append(std::to_string(quota.user_quota.max_objects));
  newAttrs.insert({"user_quota.max_objects", bl});
  bl.clear();

  bl.append(std::to_string(head_obj->get_bucket()->get_owner()->get_max_buckets()));
  newAttrs.insert({"max_buckets", bl});
  bl.clear();

  int setObjReturn = filter->get_d4n_cache()->setObject(head_obj->get_name(), &newAttrs, &attrs);

  if (setObjReturn < 0) {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache set operation failed." << dendl;
  } else {
    ldpp_dout(save_dpp, 20) << "D4N Filter: Cache set operation succeeded." << dendl;
  }
  
  return ret; // Remove later -Sam
}
*/
} } // namespace rgw::sal

extern "C" {

rgw::sal::Store* newS3Filter(rgw::sal::Store* next)
{
  rgw::sal::S3FilterStore* store = new rgw::sal::S3FilterStore(next);

  return store;
}

}
