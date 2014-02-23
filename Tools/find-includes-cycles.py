#! /usr/bin/env python

'''
Run this script from Source/Core/ to find all the #include cycles.
'''

import subprocess

def get_local_includes_for(path):
    lines = open(path).read().split('\n')
    includes = [l.strip() for l in lines if l.strip().startswith('#include')]
    return [i.split()[1][1:-1] for i in includes if '"' in i.split()[1]]

def find_all_files():
    '''Could probably use os.walk, but meh.'''
    f = subprocess.check_output(['find', '.', '-name', '*.h'],
                                universal_newlines=True).strip().split('\n')
    return [p[2:] for p in f]

def make_include_graph():
    return { f: get_local_includes_for(f) for f in find_all_files() }

def strongly_connected_components(graph):
    """
    Tarjan's Algorithm (named for its discoverer, Robert Tarjan) is a graph theory algorithm
    for finding the strongly connected components of a graph.

    Based on: http://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
    """

    index_counter = [0]
    stack = []
    lowlinks = {}
    index = {}
    result = []

    def strongconnect(node):
        # set the depth index for this node to the smallest unused index
        index[node] = index_counter[0]
        lowlinks[node] = index_counter[0]
        index_counter[0] += 1
        stack.append(node)

        # Consider successors of `node`
        try:
            successors = graph[node]
        except:
            successors = []
        for successor in successors:
            if successor not in lowlinks:
                # Successor has not yet been visited; recurse on it
                strongconnect(successor)
                lowlinks[node] = min(lowlinks[node],lowlinks[successor])
            elif successor in stack:
                # the successor is in the stack and hence in the current strongly connected component (SCC)
                lowlinks[node] = min(lowlinks[node],index[successor])

        # If `node` is a root node, pop the stack and generate an SCC
        if lowlinks[node] == index[node]:
            connected_component = []

            while True:
                successor = stack.pop()
                connected_component.append(successor)
                if successor == node: break
            component = tuple(connected_component)
            # storing the result
            result.append(component)

    for node in graph:
        if node not in lowlinks:
            strongconnect(node)

    return result

if __name__ == '__main__':
    comp = strongly_connected_components(make_include_graph())
    for c in comp:
        if len(c) != 1:
            print(c)
