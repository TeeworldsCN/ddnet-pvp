# how to play with docker image&container
# 1. build docker image

	build -t ddnet-pvp .

# 2. run a ddnet-pvp server with a container

	docker run --net=host --name=my-pvp-srv -v YOUR_PATH/autoexec.cfg:/srv/autoexec.cfg -d ddnet-pvp

* use `--net=host` could reduce little performance lost on UDP transfer.
* Highly recommand to use a extern volumn to place `autoexec.cfg`. 
* (so you could edit/manage configuration files out of container)
* example: 

	docker run --net=host --name=my-pvp-srv -v /home/teeworlds/ddnet-pvp/autoexec.cfg:/srv/autoexec/cfg -d ddnet-pvp

In this example, we could customize our server by editing the configuration file which located at `/home/teeworlds/ddnet-pvp/autoexec.cfg`.

# 3. check logs:

	docker logs my-pvp-srv

# 4. start, stop and remove:

	docker start my-pvp-srv

	docker stop my-pvp-srv

	docker rm my-pvp-srv
