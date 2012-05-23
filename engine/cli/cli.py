import os
import sys
import readline

LOG_FILENAME = '/tmp/completer.log'

def printHelp():
    print "Supporting commands:"
    print "\tlistPlugins"
    print "\tcreatePluginTemplate <pluginID>"
    print "\tremovePlugin <pluginID>"
    print "\tqueryPlugin <pluginID>"
    print "\ttestPlugin <pluginID> [--para1 val1] [--para2 val2] ..."
    print "\texecutePlugin <pluginID> [--para1 val1] [--para2 val2] ..."
    print "\tlistWorkflows <workflowID>"
    print "\tcreateWorkflow"
    print "\tremoveWorkflow <workflowID>"
    print "\tqueryWorkflow <workflowID>"
    print "\ttestWorkflow <workflowID> [--para1 val1] [--para2 val2] ..."
    print "\texecuteWorkflow <workflowID> [--para1 val1] [--para2 val2] ..."
    print "\tlistSuites"
    print "\tremoveSuite"
    print "\tclearWorkspace"

class SimpleCompleter(object):

    def __init__(self, options):
        self.options = sorted(options)
        return

    def complete(self, text, state):
        response = None
        if state == 0:
            # This is the first time for this text, so build a match list.
            if text:
                self.matches = [s 
                                for s in self.options
                                if s and s.startswith(text)]
            else:
                self.matches = self.options[:]
        
        # Return the state'th item from the match list,
        # if we have that many.
        try:
            response = self.matches[state]
        except IndexError:
            response = None
        return response


def listPlugins():
    cmd = "query_engine " + engine_root_dir + " listPlugins"
    # print cmd
    os.system(cmd)

def listWorkflows():
    cmd = "query_engine " + engine_root_dir + " listWorkflows"
    os.system(cmd)

def listSuites():
    cmd = "query_engine " + engine_root_dir + " listSuites"
    # print cmd
    os.system(cmd)

def removeSuite(it_words):
    cmd = "remove_suite " + engine_root_dir + " " + it_words.next()
    os.system(cmd)

def clearWorkspace():
    cmd = "clear_workspace " + engine_root_dir;
    os.system(cmd);

def createPluginTemplate(it_words):
    cmd = "create_plugin_template " + engine_root_dir + " " + it_words.next()
    os.system(cmd)

def removePlugin(it_words):
    cmd = "remove_plugin " + engine_root_dir + " " + it_words.next()
    os.system(cmd)

def createWorkflow(it_words):
    cmd = "create_workflow " + engine_root_dir + " " + it_words.next()
    os.system(cmd)

def removeWorkflow(it_words):
    cmd = "remove_workflow " + engine_root_dir + " " + it_words.next()
    os.system(cmd)

def queryPlugin(it_words):
    cmd = "query_engine " + engine_root_dir + " queryPlugin " + it_words.next()
    os.system(cmd)

def queryWorkflow(it_words):
    cmd = "query_engine " + engine_root_dir + " queryWorkflow " + it_words.next()
    # print cmd
    os.system(cmd)

def executePlugin(it_words, mode):
    cmd = "execute_engine " + engine_root_dir 
    # all engine parameters go here
    if mode == "test":
        cmd += " --test"
    cmd += " --debug "
    cmd += " --verbose "

    cmd += " plugin "
    while True:
        try:
            word = it_words.next()
            cmd += " " + word
        except StopIteration:
            break
    # print cmd
    os.system(cmd)


def executeWorkflow(it_words, mode):
    cmd = "execute_engine " + engine_root_dir 
    # all engine parameters go here
    if mode == "test":
        cmd += " --test"
    cmd += " --debug "
    cmd += " --verbose "

    cmd += " workflow "
    while True:
        try:
            word = it_words.next()
            cmd += " " + word
        except StopIteration:
            break
    # print cmd
    os.system(cmd)

if (len(sys.argv) != 2):
    sys.exit("Usage: python cli.py <engine_root_dir>")

engine_root_dir = sys.argv[1]
readline.set_completer(SimpleCompleter(['quit', 'help', 'listPlugins', 'listSuites', 'removeSuite', 'clearWorkspace', 'createPluginTemplate', 'removePlugin', 'listWorkflows', 'createWorkflow', 'removeWorkflow', 'queryPlugin', 'queryWorkflow', 'executePlugin', 'testPlugin', 'executeWorkflow', 'testWorkflow']).complete)
readline.parse_and_bind('tab: complete')
readline.parse_and_bind('set editing-mode vi')
print ("Enter \"exit\" to quit")

while True:
    try:
        line = raw_input('\nctc > ')
    except KeyboardInterrupt:
        continue

    if line.lower() == 'quit' or line.lower() == 'exit':
      break

    if line == "":
        continue

    words = line.split()
    it_words = iter(words)
    word = it_words.next()

    if word.lower() == 'help':
        printHelp()
        continue;

    if word == 'listPlugins':
        listPlugins()
        continue

    if word == 'listWorkflows':
        listWorkflows()
        continue

    if word == 'listSuites':
        listSuites()
        continue

    if word == 'removeSuite':
        if len(words) != 2:
            print "Usage: removeSuite <suite_id>"
        else:
            removeSuite(it_words)
        continue

    if word == 'clearWorkspace':
        clearWorkspace()
        continue

    if word == 'createPluginTemplate':
        if len(words) != 2:
            print "Usage: createPluginTemplate <plugin_id>"
        else:
            createPluginTemplate(it_words)
        continue

    if word == 'removePlugin':
        if len(words) != 2:
            print "Usage: removePlugin <plugin_id>"
        else:
            removePlugin(it_words)
        continue

    if word == 'createWorkflow':
        if len(words) != 2:
            print "Usage: createWorkflow <workflow_id>"
        else:
            createWorkflow(it_words)
        continue

    if word == 'removeWorkflow':
        if len(words) != 2:
            print "Usage: removeWorkflow <workflow_id>"
        else:
            removeWorkflow(it_words)
        continue

    if word == 'queryPlugin':
        if len(words) != 2:
            print "Usage: queryPlugin <plugin_id>"
        else:
            queryPlugin(it_words)
        continue

    if word == 'queryWorkflow':
        if len(words) != 2:
            print "Usage: queryWorkflow <workflow_id>"
        else:
            queryWorkflow(it_words)
        continue

    if word == 'executePlugin':
        if len(words) < 3:
            print "Usage: executePlugin <plugin_id> --para1 --value1 ..."
        else:
            executePlugin(it_words, "run")
        continue

    if word == 'testPlugin':
        if len(words) < 3:
            print "Usage: testPlugin <plugin_id> --para1 --value1 ..."
        else:
            executePlugin(it_words, "test")
        continue

    if word == 'executeWorkflow':
        if len(words) < 3:
            print "Usage: executeWorkflow <plugin_id> --para1 --value1 ..."
        else:
            executeWorkflow(it_words, "run")
        continue

    if word == 'testWorkflow':
        if len(words) < 3:
            print "Usage: testWorkflow <plugin_id> --para1 --value1 ..."
        else:
            executeWorkflow(it_words, "test")
        continue

    if word[0] == '!':
        os.system(' '.join(words)[1:])
        continue
    print 'Command not supported: "%s"' % line
