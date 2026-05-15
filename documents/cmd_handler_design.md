┌──────────────┐    MsgBody   ┌─────────────────┐   business call ┌──────────────┐
│ Package_recv │ ───────────► │   Dispatcher    │ ─────────────► │   Handlers   │
│              │              │ (route by cmd)  │                │ (LOGIN/QUERY)│
└──────────────┘              └─────────────────┘                └──────────────┘
                                       │                                  │
                                       │ MsgBody (response)               │
                                       ◄──────────────────────────────────┘
                                       │
                              ┌──────────────┐
                              │ Package_send │
                              └──────────────┘



┌─────────────────────────────────────────────────────┐
│  L4 Connection Loop (per-thread)                    │
│      while(alive) { recv -> dispatch -> send }      │
├─────────────────────────────────────────────────────┤
│  L3 Dispatcher                                      │
│      - Route cmd_type to its Handler                │
│      - Authorization check (admin / student)        │
│      - Convert exceptions into error responses      │
├─────────────────────────────────────────────────────┤
│  L2 Handlers (LoginHandler / QueryHandler / ...)    │
│      - Parse business payload (JSON)                │
│      - Call Database                                │
│      - Build response payload                       │
├─────────────────────────────────────────────────────┤
│  L1 Database / Domain (CourseRepository, ...)       │
└─────────────────────────────────────────────────────┘
