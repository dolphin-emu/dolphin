#! /usr/bin/env python3

'''
Run this script from Source/Core/ to find all the #include cycles.
'''

from typing import Iterator
from pathlib import Path

def get_local_includes_for(path: Path) -> Iterator[str]:
    with path.open() as file:
        for line in file:
            line = line.strip()
            if line.startswith("#include") and '"' in line:
                yield line.split()[1].strip(' "')

def strongly_connected_components(graph: dict[str, list[str]]) -> list[tuple[str, ...]]:
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

    def strongconnect(node: str) -> None:
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
    paths = Path(".").glob("**/*.h")
    graph = {
        path.as_posix(): list(get_local_includes_for(path))
        for path in paths
    }
    components = strongly_connected_components(graph)
    for component in components:
        if len(component) != 1:
            print(component)
