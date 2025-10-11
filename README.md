# Dependencies

## Debian/Ubuntu

```
sudo apt install libgrpc++-dev protobuf-compiler protobuf-compiler-grpc libprotobuf-dev libcurl4-openssl-dev libboost-json-dev libyaml-cpp-dev
```


./act_runner-0.2.13-linux-amd64 --config gitea_runner_config.yaml daemon
curl -v http://192.168.2.23:4000/api/actions/ping.v1.PingService/Ping -H "Content-Type: application/json" --json '{"data": "sl-workstation"}'
