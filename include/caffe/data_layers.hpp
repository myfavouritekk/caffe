#ifndef CAFFE_DATA_LAYERS_HPP_
#define CAFFE_DATA_LAYERS_HPP_

#include <string>
#include <utility>
#include <vector>

#include "boost/scoped_ptr.hpp"
#include "hdf5.h"
#include "leveldb/db.h"
#include "lmdb.h"

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/data_transformer.hpp"
#include "caffe/filler.hpp"
#include "caffe/internal_thread.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"

namespace caffe {

#define HDF5_DATA_DATASET_NAME "data"
#define HDF5_DATA_LABEL_NAME "label"

/**
 * @brief Provides base for data layers that feed blobs to the Net.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class BaseDataLayer : public Layer<Dtype> {
 public:
  explicit BaseDataLayer(const LayerParameter& param);
  virtual ~BaseDataLayer() {}
  // LayerSetUp: implements common data layer setup functionality, and calls
  // DataLayerSetUp to do special data layer setup for individual layer types.
  // This method may not be overridden except by the BasePrefetchingDataLayer.
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}
  // Data layers have no bottoms, so reshaping is trivial.
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}

  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}
  virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}

  int datum_channels() const { return datum_channels_; }
  int datum_height() const { return datum_height_; }
  int datum_width() const { return datum_width_; }
  int datum_size() const { return datum_size_; }

 protected:
  TransformationParameter transform_param_;
  DataTransformer<Dtype> data_transformer_;
  int datum_channels_;
  int datum_height_;
  int datum_width_;
  int datum_size_;
  Blob<Dtype> data_mean_;
  const Dtype* mean_;
  Caffe::Phase phase_;
  bool output_labels_;
};

template <typename Dtype>
class BasePrefetchingDataLayer :
    public BaseDataLayer<Dtype>, public InternalThread {
 public:
  explicit BasePrefetchingDataLayer(const LayerParameter& param)
      : BaseDataLayer<Dtype>(param) {}
  virtual ~BasePrefetchingDataLayer() {}
  // LayerSetUp: implements common data layer setup functionality, and calls
  // DataLayerSetUp to do special data layer setup for individual layer types.
  // This method may not be overridden.
  void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual void CreatePrefetchThread();
  virtual void JoinPrefetchThread();
  // The thread's function
  virtual void InternalThreadEntry() {}

 protected:
  Blob<Dtype> prefetch_data_;
  Blob<Dtype> prefetch_label_;
};

template <typename Dtype>
class DataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit DataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param) {}
  virtual ~DataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int MinTopBlobs() const { return 1; }
  virtual inline int MaxTopBlobs() const { return 2; }

 protected:
  virtual void InternalThreadEntry();

  // LEVELDB
  shared_ptr<leveldb::DB> db_;
  shared_ptr<leveldb::Iterator> iter_;
  // LMDB
  MDB_env* mdb_env_;
  MDB_dbi mdb_dbi_;
  MDB_txn* mdb_txn_;
  MDB_cursor* mdb_cursor_;
  MDB_val mdb_key_, mdb_value_;
};

template <typename Dtype>
class VectorLabelDataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit VectorLabelDataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param) {}
  virtual ~VectorLabelDataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_VECTOR_LABEL_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int MinTopBlobs() const { return 1; }
  virtual inline int MaxTopBlobs() const { return 2; }

 protected:
  virtual void InternalThreadEntry();

  // LEVELDB
  shared_ptr<leveldb::DB> db_;
  shared_ptr<leveldb::Iterator> iter_;
};

template <typename Dtype>
class SamplingVectorLabelDataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit SamplingVectorLabelDataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param) {}
  virtual ~SamplingVectorLabelDataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_SAMPLING_VECTOR_LABEL_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int MinTopBlobs() const { return 1; }
  virtual inline int MaxTopBlobs() const { return 3; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void InternalThreadEntry();
  virtual unsigned int PrefetchRand();
  shared_ptr<Caffe::RNG> prefetch_rng_;

  // LEVELDB
  shared_ptr<leveldb::DB> db_;
  shared_ptr<leveldb::Iterator> iter_;
  
  // Sampling pool
  vector<vector<std::string> >sampling_pool;
  
  // Sampling indices
  int minIdx_;
  int maxIdx_;
  int curIdx_;

};

template<typename Dtype>
class MapDataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit MapDataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param),
        label_transformer_(label_trans_param(param.transform_param())) {}
  virtual ~MapDataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_MAP_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int ExactNumTopBlobs() const { return 2; }

 protected:
  virtual void InternalThreadEntry();
  DataTransformer<Dtype> label_transformer_;

  // LEVELDB
  shared_ptr<leveldb::DB> db_;
  shared_ptr<leveldb::Iterator> iter_;
  // LMDB
  MDB_env* mdb_env_;
  MDB_dbi mdb_dbi_;
  MDB_txn* mdb_txn_;
  MDB_cursor* mdb_cursor_;
  MDB_val mdb_key_, mdb_value_;
  
  const Dtype* label_mean_;

 private:
  static TransformationParameter label_trans_param(
            const TransformationParameter& trans_param);
};

/**
 * @brief Provides data to the Net generated by a Filler.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class DummyDataLayer : public Layer<Dtype> {
 public:
  explicit DummyDataLayer(const LayerParameter& param)
      : Layer<Dtype>(param) {}
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  // Data layers have no bottoms, so reshaping is trivial.
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_DUMMY_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int MinTopBlobs() const { return 1; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}
  virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}

  vector<shared_ptr<Filler<Dtype> > > fillers_;
  vector<bool> refill_;
};

/**
 * @brief Provides data to the Net from HDF5 files.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class HDF5DataLayer : public Layer<Dtype> {
 public:
  explicit HDF5DataLayer(const LayerParameter& param)
      : Layer<Dtype>(param) {}
  virtual ~HDF5DataLayer();
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  // Data layers have no bottoms, so reshaping is trivial.
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_HDF5_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int ExactNumTopBlobs() const { return 2; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}
  virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom) {}
  virtual void LoadHDF5FileData(const char* filename);

  std::vector<std::string> hdf_filenames_;
  unsigned int num_files_;
  unsigned int current_file_;
  hsize_t current_row_;
  Blob<Dtype> data_blob_;
  Blob<Dtype> label_blob_;
};

/**
 * @brief Write blobs to disk as HDF5 files.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class HDF5OutputLayer : public Layer<Dtype> {
 public:
  explicit HDF5OutputLayer(const LayerParameter& param);
  virtual ~HDF5OutputLayer();
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}
  // Data layers have no bottoms, so reshaping is trivial.
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {}

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_HDF5_OUTPUT;
  }
  // TODO: no limit on the number of blobs
  virtual inline int ExactNumBottomBlobs() const { return 2; }
  virtual inline int ExactNumTopBlobs() const { return 0; }

  inline std::string file_name() const { return file_name_; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);
  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom);
  virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, vector<Blob<Dtype>*>* bottom);
  virtual void SaveBlobs();

  std::string file_name_;
  hid_t file_id_;
  Blob<Dtype> data_blob_;
  Blob<Dtype> label_blob_;
};

/**
 * @brief Provides data to the Net from image files.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class ImageDataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit ImageDataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param) {}
  virtual ~ImageDataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_IMAGE_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int ExactNumTopBlobs() const { return 2; }

 protected:
  shared_ptr<Caffe::RNG> prefetch_rng_;
  virtual void ShuffleImages();
  virtual void InternalThreadEntry();

  vector<std::pair<std::string, int> > lines_;
  int lines_id_;
};

/**
 * @brief Provides data to the Net from memory.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class MemoryDataLayer : public BaseDataLayer<Dtype> {
 public:
  explicit MemoryDataLayer(const LayerParameter& param)
      : BaseDataLayer<Dtype>(param), has_new_data_(false) {}
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_MEMORY_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int ExactNumTopBlobs() const { return 2; }

  virtual void AddDatumVector(const vector<Datum>& datum_vector);

  // Reset should accept const pointers, but can't, because the memory
  //  will be given to Blob, which is mutable
  void Reset(Dtype* data, Dtype* label, int n);

  int batch_size() { return batch_size_; }

 protected:
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  int batch_size_;
  Dtype* data_;
  Dtype* labels_;
  int n_;
  int pos_;
  Blob<Dtype> added_data_;
  Blob<Dtype> added_label_;
  bool has_new_data_;
};

/**
 * @brief Provides data to the Net from windows of images files, specified
 *        by a window data file.
 *
 * TODO(dox): thorough documentation for Forward and proto params.
 */
template <typename Dtype>
class WindowDataLayer : public BasePrefetchingDataLayer<Dtype> {
 public:
  explicit WindowDataLayer(const LayerParameter& param)
      : BasePrefetchingDataLayer<Dtype>(param) {}
  virtual ~WindowDataLayer();
  virtual void DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top);

  virtual inline LayerParameter_LayerType type() const {
    return LayerParameter_LayerType_WINDOW_DATA;
  }
  virtual inline int ExactNumBottomBlobs() const { return 0; }
  virtual inline int ExactNumTopBlobs() const { return 2; }

 protected:
  virtual unsigned int PrefetchRand();
  virtual void InternalThreadEntry();

  shared_ptr<Caffe::RNG> prefetch_rng_;
  vector<std::pair<std::string, vector<int> > > image_database_;
  enum WindowField { IMAGE_INDEX, LABEL, OVERLAP, X1, Y1, X2, Y2, NUM };
  vector<vector<float> > fg_windows_;
  vector<vector<float> > bg_windows_;
};

}  // namespace caffe

#endif  // CAFFE_DATA_LAYERS_HPP_
