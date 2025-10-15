#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <wx/event.h>

namespace  HMS
{
   
//  事件的载体
class GUIActionAdapte
{     
    public:
      GUIActionAdapte() = default;
      virtual ~GUIActionAdapte() = default;
};

// ===============================
// 事件处理接口：模拟 osgGA::GUIEventHandler
// ===============================
class GUIEventHandler {
public:
    virtual bool handle(const wxEvent& ea,GUIActionAdapte* aa) = 0;
    virtual ~GUIEventHandler() = default;
};


// ===============================
// 事件处理管理器：模拟 osgViewer::Viewer 添加/分发事件处理器
// ===============================
class EventManager {
public:
    using EventHandlerPtr = std::shared_ptr<GUIEventHandler>;

    void addEventHandler(EventHandlerPtr handler);
    
    // 模拟事件分发
    void dispatchEvent(const wxEvent& event, GUIActionAdapte* aa);

private:
    std::vector<EventHandlerPtr> _handlers;
};

}
