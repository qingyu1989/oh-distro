#ifndef __IMUMessageProducer_h
#define __IMUMessageProducer_h

#include "LCMSubscriber.h"
#include "QueueTypes.h"
#include "IMUBatchProcessor.h"
#include "IMUFilter.h"


namespace StateEstimate
{

class IMUMessageProducer : public LCMSubscriber
{
public:

  IMUMessageProducer(const std::string& IMURxChannel, const std::string& ERSTxChannel) : mChannel(IMURxChannel), mIMUFilter(ERSTxChannel)
  {

  }

  virtual void subscribe(boost::shared_ptr<lcm::LCM> lcmHandle)
  {
    lcmHandle->subscribe(this->mChannel, &IMUMessageProducer::messageHandler, this);
    lcmHandle->subscribe("POSE_BDI", &IMUMessageProducer::poseMessageHandler, this);
  }

  IMUQueue& messageQueue()
  {
    return mQueue;
  }

  IMUFilter* getIMUFilter() {
	return &mIMUFilter;
  }


  void setSpecialLCMPtr(boost::shared_ptr<lcm::LCM> lcmHandle) {
	  mIMUFilter.setLCMPtr(lcmHandle);
  }

protected:

  void messageHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const drc::atlas_raw_imu_batch_t* msg)
  {
    VarNotUsed(rbuf);
    VarNotUsed(channel);

    std::vector<drc::atlas_raw_imu_t> imuPackets;
    this->mBatchProcessor.handleIMUBatchMessage(msg, imuPackets);

    this->mIMUFilter.handleIMUPackets(imuPackets, lastPose);

    for (size_t i = 0; i < imuPackets.size(); i++)
    {
      //std::cout << "IMUMessageProducer::messageHandler -- enqueue" << std::endl;
      mQueue.enqueue(imuPackets[i]);
    }

  }
  
  void poseMessageHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const bot_core::pose_t* msg)
    {
      VarNotUsed(rbuf);
      VarNotUsed(channel);

      lastPose = *msg;

    }

  std::string mChannel;
  boost::shared_ptr<lcm::LCM> mLCM;
  IMUQueue mQueue;

  IMUBatchProcessor mBatchProcessor;
  IMUFilter mIMUFilter;
  bot_core::pose_t lastPose;

};




} // end namespace

#endif // __IMUMessageProducer_h