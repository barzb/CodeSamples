#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#include <vector>
#include <cmath>


#include "MoveAction.h"

class Creature
{
private:
	sf::Vector2u* windowSize;
	Creature* partner;

	sf::CircleShape body;
	sf::CircleShape sight;
	sf::Vector2f position;
	MoveAction moveAction;

	// chromosom relevant parameters
	float size;
	float sightRadius;
	int timeToLive;
	int timeToReplicate;
	int replicationDuration;

	unsigned int lifeTime;
	
  // states
	bool alive;
	bool dying;
	bool replicating;
	bool movingToPartner;

  // unique id
	int id;

public:
  // static ID counter
	static int ID;

  // constructors
	Creature(sf::Vector2u&);
	Creature(sf::Vector2u&, Creature*, Creature*);

  // Methods
	void init();
	void update(int delta);
	void draw(sf::RenderWindow&) const;

	void searchPartner(std::vector<Creature*>&);

	void finishReplicating();

	void partnerDied();
	void setPartner(Creature* p) { partner = p; }

	bool collides(Creature*);

  // GETTERS
	bool isAlive() { return alive; }
	bool isDying() { return dying; }
	bool isReplicating();
	bool isMovingToPartner() { return movingToPartner; }
	bool isReadyToReplicate() { return lifeTime > timeToReplicate; }
	const sf::Vector2f& getPosition() { return position; }
	float getRadius() { return size; }
	float getSightRadius() { return sightRadius; }
	float getSize() { return size; }
	sf::Color getColor() { return body.getFillColor(); }
	int getTTL() { return timeToLive; }
	int getTTR() { return timeToReplicate; }
	int getId() { return id; }
	int getReplicationDuration() { return replicationDuration; }
	Creature* getPartner() { return partner; }
};

