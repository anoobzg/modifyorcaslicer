#include "GUIEventHandler.hpp"

namespace  HMS
{

void EventManager::addEventHandler(EventHandlerPtr handler) 
{
    _handlers.push_back(handler);
}

void EventManager::dispatchEvent(const wxEvent& event, GUIActionAdapte* aa) 
{
    for (auto& handler : _handlers) {
        if (handler->handle(event,aa)) {
            // 如果返回 true，表示事件已被消费，不再继续传递
            break;
        }
    }
}

};