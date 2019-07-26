#unplug un hub and removes all channels where it is present
import json
import numpy as np
import sys
from collections import Counter

np.random.seed(1992)

input_args = list(sys.argv)

hub_position = 0

with open(input_args[1], 'rb') as input:
    graph = json.load(input)

    # FIND AN HUB
    pub_keys = []
    for edge in graph["edges"]:
        pub_keys.append(edge["node1_pub"])
        pub_keys.append(edge["node2_pub"])

    # map_nodes = {}
    # node_id = 0
    # for pk in set(pub_keys):
    #     map_nodes[pk] = node_id
    #     node_id += 1

    # nodes = []
    # for pk in pub_keys:
    #     nodes.append(map_nodes[pk])

    channels_per_node = Counter(pub_keys).most_common()
    hub = channels_per_node[hub_position][0]

    print 'hub pk: ' + hub

    print 'previous number of channels: ' + str(len(graph["edges"]))
    graph["edges"] = [edge for edge in graph["edges"] if edge["node1_pub"]!=hub and edge["node2_pub"]!=hub]
    print 'new number of channels: ' + str(len(graph["edges"]))

    ## TEST
    for edge in graph["edges"]:
        if edge["node1_pub"] == edge["node2_pub"]:
            print 'ERROR: auto-channel'
            sys.exit(0)
        if  edge["node1_pub"] == hub or  edge["node2_pub"] == hub:
            print 'ERROR: hub still present'
            sys.exit(0)

with open('ln-without-hub-'+ str(hub_position) + '.json', 'wb') as output:
    json.dump(graph, output, indent=2)

    print 'file written without errors'

    # tot_channels = sum(channels_per_node.values())
    # print tot_channels
    # percentile = tot_channels*10/100;

    # n_channels = 0
    # hubs = []
    # for node,channels in channels_per_node.most_common():
    #     n_channels += channels
    #     if n_channels > percentile:
    #         break
    #     hubs.append(node)

    # print len(hubs)