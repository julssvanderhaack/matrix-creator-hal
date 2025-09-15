To install and use mosquitto we need to

```sh
sudo apt install mosquitto mosquitto-clients mosquitto-dev
sudo -EH /usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf -d
```

To allow remote connections add this to the config file

```
listener 1883
allow_anonymous true

```

To test use this commands in two different terminals

```sh
mosquitto_sub -h <ip> -t topic
mosquitto_pub -h <ip> -t topic -m "Hello"
```

where `<ip>` is the public ip of the server
