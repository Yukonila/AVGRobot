#include "taskmanager.h"

TaskManager::TaskManager(QObject *parent)
    : QObject{parent}
{
}

TaskManager::~TaskManager()
{
    clearAll();
}

bool TaskManager::addTask(const Task &task)
{
    return false;
}
