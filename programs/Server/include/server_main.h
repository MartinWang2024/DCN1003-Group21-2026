#pragma once

#include "CmdHandler.h"
#include "SocketHandler.h"
#include "database.h"

constexpr int PORT    = 9001;
constexpr int BACKLOG = 10;

// Database file path (relative to the executable's working directory;
// run from the project root, or change to an absolute path).
// Single DCN.db hosting courses/schedules/administrators.
constexpr const char* DB_COURSES_PATH = "data/DCN.db";
constexpr const char* DB_ADMINS_PATH  = "data/DCN.db";

// Per-connection worker thread.
// sh takes ownership of the socket via move; courses/admins/dispatcher are references to the main instances.
void handle_client(TcpSocket::SocketHandler sh,
                   dcn_database::CourseRepository& courses,
                   dcn_database::AdministratorRepository& admins,
                   Protocal::Dispatch::Dispatcher& dispatcher);
