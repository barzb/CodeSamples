#include "Creature.h"

int Creature::ID = 0;

// CONSTRUCTOR FOR PARENTS (initial creatures)
Creature::Creature(sf::Vector2u& w)
	: moveAction(w, position)
{
	windowSize = &w;
	init();

  // RANDOMIZE ATTRIBUTES
	size = rand()%5 + 10.f;
	sightRadius = size + rand()%100;
	
	position = sf::Vector2f(rand()%windowSize->x, rand()%windowSize->y);
	
	body.setFillColor(sf::Color(rand()%255, rand()%255, rand()%255, 200));
	
	timeToLive = rand()%10000 + 100;
	timeToReplicate = rand()%1200 + 200;
	replicationDuration = rand()%1000 + 200;
}

// CONSTRUCTOR FOR BABIES (born creatures)
Creature::Creature(sf::Vector2u& w, Creature* dad, Creature* mum)
	: moveAction(w, position)
{
	windowSize = &w;
	init();
  
  
  // POSITION OF MUM
	position = sf::Vector2f(mum->getPosition().x, mum->getPosition().y);
  
  // INHERIT CHARACTERISTICS FROM MUM OR DAD CREATURE

  // SIZE: INTERPOLATION OF MUM AND DAD
	int mutationRisk = rand()%100;
	size = (dad->getSize() + mum->getSize()) / 2;
	if(mutationRisk > 95) size = rand()%5 + 10.f;

  // SIGHT: INTERPOLATION OF MUM AND DAD
	mutationRisk = rand()%100;
	sightRadius = (dad->getSightRadius() + mum->getSightRadius()) / 2;
	if(mutationRisk > 95) sightRadius = size + rand()%100;

  // COLOR: RANDOM MUM OR DAD
	mutationRisk = rand()%100;
	body.setFillColor((rand()%2 == 0) ? dad->getColor() : mum->getColor());
	if(mutationRisk > 95) body.setFillColor(sf::Color(rand()%255, rand()%255, rand()%255, 200));

  // LIFETIME: RANDOM MUM OR DAD
	mutationRisk = rand()%100;
	timeToLive = (rand()%2 == 0) ? dad->getTTL() : mum->getTTL();
	if(mutationRisk > 95) timeToLive = rand()%10000 + 100;

  // REPLICATION TIMER: RANDOM MUM OR DAD
	mutationRisk = rand()%100;
	timeToReplicate = (rand()%2 == 0) ? dad->getTTR() : mum->getTTR();
	if(mutationRisk > 95) timeToReplicate = rand()%800 + 200;

  // REPLICATION DURATION:INTERPOLATION OF MUM AND DAD
	mutationRisk = rand()%100;
	replicationDuration = (dad->getReplicationDuration() + mum->getReplicationDuration()) / 2;
	if(mutationRisk > 95) replicationDuration = rand()%1000 + 200;
}

// INIT CREATURE WITH ATTRIBUTES
void Creature::init()
{
	partner = NULL;

	body = sf::CircleShape(0.01f);
	sight = sf::CircleShape(0.01f);

	body.setPosition(position);
	sight.setPosition(position);

	body.setOutlineColor(sf::Color::White);
	sight.setFillColor(sf::Color(200, 200, 200, 50));

	alive = true;
	dying = false;
	replicating = false;
	movingToPartner = false;

	lifeTime = 0;
	id = ID++;
}


// COLLISION WITH OTHER CREATURE
bool Creature::collides(Creature* c)
{
	sf::Vector2f p = c->getPosition();
	float r = c->getRadius();
  
  // CHECK IF IN SIGHT
	if ( p.x + r + sightRadius > position.x
		&& p.x < position.x + r + sightRadius
		&& p.y + r + sightRadius > position.y
		&& p.y < position.y + r + sightRadius)
	{
    // PYTHAGORAS
		float distance = sqrtf(
            ((position.x - p.x) * (position.x - p.x))
          + ((position.y - p.y) * (position.y - p.y))
    );
    // IN RANGE
		if (distance < sightRadius + r)
		{
		   return true;
		}
	}
  // NOT IN RANGE
	return false;
}

// LOOK OUT FOR A CREATURE THAT IS WILLING TO REPLICATE
void Creature::searchPartner(std::vector<Creature*>& creatures)
{
	for(int i = 0; i < creatures.size(); ++i)
	{
    // I DON'T HAVE A PARTNER YET :(
		if(partner == NULL || !partner->isReadyToReplicate() || !collides(partner))
		{
      // FOUND ONE THAT IS LEGIT?
			if( creatures[i] != this && collides(creatures[i]) 
				&& creatures[i]->isReadyToReplicate() && !creatures[i]->isReplicating()
				&& (creatures[i]->getPartner() == NULL || creatures[i]->getPartner() == this))
			{
        // YAY I FOUND A PARTNER -> MOVE TOWARDS PARTNER
				moveAction.setTargetPosition((creatures[i]->getPosition() + position) / 2.f);
				sight.setFillColor(sf::Color(255, 255, 0, 100));
				partner = creatures[i];
				movingToPartner = true;
			}
		} 
    // I ALREADY HAVE A PARTNER :)
    else 
    {
      // MOVE TOWARD CREATURE
			moveAction.setTargetPosition((partner->getPosition() + position) / 2.f);
			sight.setFillColor(sf::Color(255, 255, 0, 100));
			movingToPartner = true;
		}
	}
  
  // UNHIGHLIGHT
	if(!movingToPartner)
	{
		sight.setFillColor(sf::Color(200, 200, 200, 50));
	}
}

// DOESN'T MATTER, HAD SEX
// SET COOLDOWN FOR NEXT REPLICATION
void Creature::finishReplicating()
{
	partner = NULL;
	replicating = false;
	movingToPartner = false;
	timeToReplicate += lifeTime;
	moveAction.setRandomTargetPosition();
	sight.setFillColor(sf::Color(200, 200, 200, 50));
}

// CALLED EVERY FRAME
void Creature::update(int delta)
{
  // IF POSITION IS OUT OF SCREEN -> TELEPORT TO OPPOSITE SIDE
	if(position.x < 0.f)
		position.x += windowSize->x;
	else if(position.x > windowSize->x)
		position.x -= windowSize->x;

	if(position.y < 0.f)
		position.y += windowSize->y;
	else if(position.y > windowSize->y)
		position.y -= windowSize->y;

  // STILL ALIVE
	if(++lifeTime < timeToLive)
	{
		if(body.getRadius() < size)
		{
			body.setRadius(body.getRadius() + size/10.f);
			sight.setRadius(sight.getRadius() + sightRadius/10.f);
		}
	} 
  // HE'S DEAD, JIM!
  else 
  {
		dying = true;
		if(partner != NULL) partner->partnerDied(); // :(
		body.setRadius(body.getRadius() - size/10.f);
		sight.setRadius(sight.getRadius() + sightRadius/10.f);

		if(body.getRadius() < .1f)
		{
			alive = false;
		}
	}

  // READY TO REPLICATE ? HIGHTLIGHT IT !
	if(lifeTime > timeToReplicate)
		body.setOutlineThickness(2.f);
	else
		body.setOutlineThickness(0.f);

  // STILL ALIVE 
	if(alive && !dying)
	{
    // UPDATE MOVE ACTION
		moveAction.update();
		if(moveAction.targetReached() && !replicating)
			moveAction.setRandomTargetPosition();
    // REPLICATING ?
		if(movingToPartner && partner != NULL && moveAction.targetReached(partner->getPosition()))
			replicating = true;
	}

  // SET POSITIONS OF CIRCLES
	body.setPosition(position.x - body.getRadius(), position.y - body.getRadius());
	sight.setPosition(position.x - sight.getRadius(), position.y - sight.getRadius());
}

// DRAW THE CIRCLES OF THE CREATURE
void Creature::draw(sf::RenderWindow& w) const
{
	w.draw(sight);
	w.draw(body);
}

// GETTER
bool Creature::isReplicating()
{
	return replicating && (lifeTime - timeToReplicate > replicationDuration);
}

// THIS IS JUST AWFUL... 
void Creature::partnerDied() 
{ 
	partner = NULL; 
	replicating = false;
	movingToPartner = false;
}




