import json;
import sys;

cloth_nodes=[];
cloth_channels=[];
cloth_edges=[];

def make_cloth_data(channel_id,channel):
    #channel: id,edge1_id,edge2_id,node1_id,node2_id,capacity
    length=len(cloth_channels);
    id=str(length);
    edge1_id=str(length*2);
    edge2_id=str(length*2+1)
    
    if channel[channel_id]["node1_pub"] not in cloth_nodes:
        cloth_nodes.append(channel[channel_id]["node1_pub"]);
    node1_id=str(cloth_nodes.index(channel[channel_id]["node1_pub"]));

    if channel[channel_id]["node2_pub"] not in cloth_nodes:
        cloth_nodes.append(channel[channel_id]["node2_pub"]);
    node2_id=str(cloth_nodes.index(channel[channel_id]["node2_pub"]));
        
    capacity = channel[channel_id]["capacity"];
    cloth_channels.append(
        id+","+
        edge1_id+","+
        edge2_id+","+
        node1_id+","+
        node2_id+","+
        capacity
    );

    balance=int(capacity);
    balance1=balance-(balance//2);
    balance2=balance-balance1;

    #id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock
    cloth_edges.append(
        edge1_id+","+
        id+","+
        edge2_id+","+
        node1_id+","+
        node2_id+","+
        str(balance1)+","+
        channel[channel_id]["node1_policy"]["fee_base_msat"]+","+
        channel[channel_id]["node1_policy"]["fee_rate_milli_msat"]+","+
        channel[channel_id]["node1_policy"]["min_htlc"]+","+
        str(channel[channel_id]["node1_policy"]["time_lock_delta"])
    );
    cloth_edges.append(
        edge2_id+","+
        id+","+
        edge1_id+","+
        node2_id+","+
        node1_id+","+
        str(balance2)+","+
        channel[channel_id]["node2_policy"]["fee_base_msat"]+","+
        channel[channel_id]["node2_policy"]["fee_rate_milli_msat"]+","+
        channel[channel_id]["node2_policy"]["min_htlc"]+","+
        str(channel[channel_id]["node2_policy"]["time_lock_delta"])
    );

def translate(file):
    f = open(file, 'r');
    channels = json.load(f);
    none_count=0;
    none_channels=[];
    print("channel count ",len(channels));
    for c in channels.keys():
        if channels[c]["node1_policy"]["fee_base_msat"] == None or channels[c]["node2_policy"]["fee_base_msat"] == None:
            none_count+=1;
            none_channels.append(c);
            continue;
    for c in none_channels:
        channels.pop(c);
    print("channel count ",len(channels));
    print("none count ", none_count);


    for c in channels.keys():
        make_cloth_data(c,channels);

    with open("cloth_nodes.csv", mode='w') as f:
        f.write("id\n")
        for i in range(len(cloth_nodes)):
            f.write(str(i)+"\n");

    with open("cloth_channels.csv", mode='w') as f:
        f.write("id,edge1_id,edge2_id,node1_id,node2_id,capacity\n");
        for i in range(len(cloth_channels)):
            f.write(cloth_channels[i]+"\n");
    
    with open("cloth_edges.csv", mode='w') as f:
        f.write("id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock\n");
        for i in range(len(cloth_edges)):
            f.write(cloth_edges[i]+"\n");

    #print(cloth_nodes);
    #print(cloth_channels);
    #print(cloth_edges);

def main():
    translate("channel_1ml_20231117.json");
    
if __name__ == "__main__":
    main();

    """
    channel: id,edge1_id,edge2_id,node1_id,node2_id,capacity
    edge: id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock
    
    "812793080224940032": {
    "channel_id": "812793080224940032",
    "chan_point": "e6642c962b7f56d69e53a19ddebcd801f9635ee0706ed2451fa7193935432045:0",
    "last_update": 1682106498,
    "node1_pub": "033d9e73a183c9714545f292875fb90c4372bddc9c2cc302b265d15e7969a5ed60",
    "node2_pub": "035715b716b8d61b31992256194f119e1cd7f86b5a15f66ca4cc3798d341d6a99a",
    "capacity": "10000000",
    "node1_policy": {
      "time_lock_delta": 144,
      "min_htlc": "1000",
      "fee_base_msat": "0",
      "fee_rate_milli_msat": "80"
    },
    "node2_policy": {
      "time_lock_delta": 40,
      "min_htlc": "1000",
      "fee_base_msat": "0",
      "fee_rate_milli_msat": "47"
    }
  }
    """