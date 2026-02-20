#ifndef COMMAND_H
#define COMMAND_H

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Runtime& rt) = 0;
};

#endif