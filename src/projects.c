#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "projects.h"

#define PROJECTS_FILE "data/projects.json" // path to the JSON file on disk

// ── projects_load ──────────────────────────────────────────────────────────
// Opens projects.json and reads each project into the list array.
// This is not a full JSON parser — it reads line by line looking for
// specific keys. This works because we control the format of the file.
// Parameters:
// list = pointer to an array of Project structs we will fill in
// max  = maximum number of projects to load (prevents overflow)
// Returns: number of projects loaded
int projects_load(Project *list, int max) {
    FILE *f = fopen(PROJECTS_FILE, "r"); // open for reading
    if (!f) return 0; // file does not exist yet — return 0 projects, that is fine

    int count = 0;
    char line[1024];
    Project current;
    memset(&current, 0, sizeof(Project)); // zero the struct before filling it

    // fgets() reads one line at a time from the file into 'line'
    // it stops at newline or end of file
    // the while loop keeps reading until end of file or we hit max projects
    while (fgets(line, sizeof(line), f) && count < max) {

        // strstr() searches for a substring inside a string
        // returns a pointer to where it was found, or NULL if not found
        // we use it to detect which key this line contains

        if (strstr(line, "\"name\"")) {
            // line looks like:   "name": "My Flask App",
            // %255[^"] means: read up to 255 characters, stop at closing quote
            sscanf(line, " \"name\": \"%255[^\"]\"", current.name);

        } else if (strstr(line, "\"path\"")) {
            sscanf(line, " \"path\": \"%511[^\"]\"", current.path);

        } else if (strstr(line, "\"command\"")) {
            sscanf(line, " \"command\": \"%255[^\"]\"", current.command);

        } else if (strstr(line, "\"db_port\"")) {
            // %d reads an integer — no quotes around numbers in JSON
            // &current.db_port = address of the field so sscanf can write into it
            sscanf(line, " \"db_port\": %d", &current.db_port);

        } else if (strstr(line, "}")) {
            // closing brace means we finished reading one project object
            // only save it if we actually got a name
            if (strlen(current.name) > 0) {
                list[count++] = current; // copy current into the list, increment count
                memset(&current, 0, sizeof(Project)); // reset for the next project
            }
        }
    }

    fclose(f);
    return count; // tell the caller how many projects we found
}

// ── projects_list_json ─────────────────────────────────────────────────────
// Loads all projects then builds a JSON array string from them.
// The router calls this when the browser requests /api/projects.
// Parameters:
// out      = char array to write the finished JSON string into
// out_size = size of that array so we do not overflow it
void projects_list_json(char *out, int out_size) {
    Project list[MAX_PROJECTS];
    int count = projects_load(list, MAX_PROJECTS);

    // start the JSON array
    strncpy(out, "[\n", out_size);

    for (int i = 0; i < count; i++) {
        char entry[768];
        // sprintf builds one JSON object string for this project
        // the ternary at the end adds a comma after every entry except the last
        // because valid JSON does not allow a trailing comma
        sprintf(entry,
            "  {\"name\":\"%s\",\"path\":\"%s\",\"command\":\"%s\",\"db_port\":%d}%s\n",
            list[i].name,
            list[i].path,
            list[i].command,
            list[i].db_port,
            i < count - 1 ? "," : "" // comma after all except last entry
        );
        // strncat appends entry to out, stopping before out_size is exceeded
        strncat(out, entry, out_size - strlen(out) - 1);
    }

    strncat(out, "]", out_size - strlen(out) - 1); // close the JSON array
}

// ── projects_add ───────────────────────────────────────────────────────────
// Appends one new project to projects.json.
// We load all existing projects, add the new one, then rewrite the whole file.
// Parameter: p = pointer to the Project struct to add
// Returns: 1 = success, 0 = failure
int projects_add(Project *p) {
    Project list[MAX_PROJECTS];
    int count = projects_load(list, MAX_PROJECTS);

    if (count >= MAX_PROJECTS) return 0; // list is full

    list[count++] = *p; // add the new project at the end, increment count
    // *p dereferences the pointer — copies the actual Project data, not just the address

    // rewrite the entire JSON file with the updated list
    FILE *f = fopen(PROJECTS_FILE, "w"); // "w" = write mode, creates or overwrites
    if (!f) return 0;

    fprintf(f, "[\n"); // start of JSON array
    for (int i = 0; i < count; i++) {
        fprintf(f,
            "  {\n"
            "    \"name\": \"%s\",\n"
            "    \"path\": \"%s\",\n"
            "    \"command\": \"%s\",\n"
            "    \"db_port\": %d\n"
            "  }%s\n",
            list[i].name,
            list[i].path,
            list[i].command,
            list[i].db_port,
            i < count - 1 ? "," : ""
        );
    }
    fprintf(f, "]\n"); // end of JSON array
    fclose(f);
    return 1; // success
}