/*!
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * \file      OranrrhHandlerORanOperations.h
 * \brief     <one-line description of file>
 *
 *
 * \details   <multi-line detailed description of file>
 *
 */


#ifndef INC_ORANRRHHANDLERORANOPERATIONS_H_
#define INC_ORANRRHHANDLERORANOPERATIONS_H_

#include "YangHandlerSysrepo.h"
#include "SysrepoGetitemsCallback.h"

namespace Mplane
{

/*!
 * \class  OranrrhHandlerORanOperations
 * \brief
 * \details
 *
 */
class OranrrhHandlerORanOperations : public YangHandlerSysrepo
{
public:
	OranrrhHandlerORanOperations(std::shared_ptr<IYangModuleMgr> moduleMgr);

	/*
	 * Run the initialisation of the handler (can only be done once the rest of the YANG framework is up)
	 */
	virtual bool initialise() override;

	/**
	 * Module change hook - called by module_change() method with filtered events
	 */
	virtual void valueChange(const std::string& xpath,
	                         std::shared_ptr<YangParam> oldValue,
	                         std::shared_ptr<YangParam> newValue);
protected:

private:
	void callTzSet();
	bool rpcReset(std::shared_ptr<sysrepo::Session> session,
				  const std::string &rpcXpath,
				  std::shared_ptr<YangParams> callList,
				  std::shared_ptr<YangParams> retList);

	std::shared_ptr<SysrepoGetitemsCallback> mCallback;
};

}

#endif /* _INC_ORANRRHHANDLERORANOPERATIONS_H_ */
