#ifndef PROJECTS_H
#define PROJECTS_H

// represents one project entry — mirrors one object in projects.json
typedef struct {
    char name[256];     // project display name
    char path[512];     // folder path on disk
    char command[256];  // command to run it e.g. "python main.py"
    int  db_port;       // database port to check before running (0 = none)
} Project;

#define MAX_PROJECTS 50 // maximum number of projects we support

// loads all projects from projects.json into the list array
// returns how many projects were loaded
int projects_load(Project *list, int max);

// builds a JSON string from the project list and writes it into 'out'
void projects_list_json(char *out, int out_size);

// adds one new project to projects.json
// returns 1 if successful, 0 if failed
int projects_add(Project *p);

#endif