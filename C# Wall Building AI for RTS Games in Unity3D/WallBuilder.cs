using UnityEngine;
using System.Collections;
using System.Collections.Generic;

// grass = 0, building = 1, wall = 2, rock = 3, main = 4

public class WallBuilder : MonoBehaviour {

	// wall builder params
	public int minWallDistance = 1;		// minimum distance to start tile
	public int maxWallTiles = 100;		// maximum number wall tiles
	public int constantValue = 2;		// constant value for heuristic function
	public int stepsIntoFuture = 100;	// number of steps (after wall was found) to find better wall
	public GUIText gui;					// gui screen top  for user information

	// map reference
	private TileMap map;

	// lists
	private Node[,] nodes;			// list with all nodes
	private List<Node> open;		// list with expanded nodes
	private List<Node> closed;		// list with interior nodes
	private List<Node> openCopy;	// list with nodes for later wall

	// step counters
	private int futureSteps;	
	private int buildCounter;

	// start node reference
	private Node startNode;

	// width and height of node array
	private int w;
	private int h;

	// switches
	private bool building = false;			// if algorithm is processing
	private bool automaticMode = false;		// if steps perform automatically
	private bool resetted = true;			// if wall builder was changed or freshly resetted

	// Use this for initialization
	void Start () {
		// get map with size
		map = GameObject.FindGameObjectWithTag("MAP").GetComponent<TileMap>();
		w = map.width;
		h = map.height;

		// init lists
		nodes  		= new Node[w, h];
		open   		= new List<Node>();
		closed 		= new List<Node>();
		openCopy 	= new List<Node>();
	}
	
	// Update is called once per frame
	void Update () {

		// RETURN key = automatic mode
		if(Input.GetKeyDown(KeyCode.Return))
		{
			if(!building)
			{
				reset ();
				buildWall();
			}
			automaticMode = true;

		}
		// SPACE key = step by step mode
		if(Input.GetKeyDown(KeyCode.Space))
		{
			automaticMode = false;

			if(building)
				step();
			else
			{
				reset ();
				buildWall();
			}
		}

		// mouse click resets the  building process
		if(Input.GetMouseButtonDown(0) || Input.GetMouseButtonDown(1))
		{
			gui.text = "";
			automaticMode = false;

			if(!resetted)
				reset();
		}

		// automode
		if(automaticMode && building)
		{
			step();
		}
	}

	// first step of algorithm
	public void buildWall()
	{
		// reset everything
		gui.text = "";
		nodes  = new Node[w, h];
		open   = new List<Node>();
		closed = new List<Node>();
		openCopy = new List<Node>();
		futureSteps = stepsIntoFuture;
		resetted = false;					

		bool firstBuildingFound = false;	// if there are any buildings on the map
		int mostLeft = 0;					// most left node
		int mostRight = w;					// most right node
		int mostTop = 0;					// most top node
		int mostBot = h;					// most bottom node

		// convert Tiles to Nodes
		for(int i = 0; i < w; ++i)
		{
			for(int j = 0; j < h; ++j)
			{
				Tile t = map.get(i, j);

				// change wall tiles to grass tiles
				if((int)t.type == 2) 
					t.changeType(0);

				// convert to node
				nodes[i, j] = tile2Node(t);
				nodes[i, j].posX = i;
				nodes[i, j].posY = j;

				// look for buildings
				if((int)t.type == 1)
				{
					// first building  was found
					if(!firstBuildingFound)
					{
						mostLeft = i;
						mostRight = i;
                     	mostTop = j;
                     	mostBot = j;
						firstBuildingFound = true;
					} 
					else // more buildings were found
					{
						if(i < mostLeft)  mostLeft = i;
						if(i > mostRight) mostRight = i;
						if(j < mostTop)   mostTop = j;
						if(j > mostBot)   mostBot = j;
					}

				}
			}
		}

		//  in case there were no buildings on the map
		if(!firstBuildingFound)
		{
			reset();
			gui.text = "No buildings found!";
			return;
		}

		//  set start node to center of all tiles
		startNode = nodes[(int)((mostLeft+mostRight)/2) , (int)((mostBot+mostTop)/2)];
		// in case the startnode is a natural barrier; change the startnode
		int c = 0;
		while(startNode.type == 3)
		{
			startNode = nodes[(int)((mostLeft+mostRight)/2)+c , (int)((mostBot+mostTop)/2)+c];

			++c;

			if(startNode.posX >= w || startNode.posY >= h)
			{
				startNode = null;
				break;
			}
		}

		// prepare startnode for start and call step method
		if(startNode != null)
		{
			map.highlightTile(startNode.posX, startNode.posY, 3);
			startNode.listed = true;
			startNode.interior = true;
			startNode.neighbors = 4;
			open.Add(startNode);
			calculateCosts();

			building = true;
			buildCounter = 0;

			step ();
		}
	}

	// stepmethod for main loop of algorithm
	private void step()
	{
		// openlist is empty -> no wall can be built 
		if(open.Count == 0)
		{
			Debug.Log("Keiner Mauer gefunden oder Gebäude sind von Außenwelt abgeschlossen");
			reset ();

			return;
		}

		// sort openList with heuristic value
		open.Sort();

		// get node with best value
		Node next = open[0];
		// look for neighbor tiles in openList
		int nextIndex = 0;
		while(next.neighbors == 0)
		{
			next = open[++nextIndex];
		}
		// move tile from openList to  closedList
		open.RemoveAt(nextIndex);
		closed.Add(next);
		next.interior = true;
		// expand node
		expand (next);
		// highlight (change color of tile)
		map.highlightTile(next.posX, next.posY, 1);

		// increment counter
		buildCounter++;

		// check if wall criteria are met and finish process
		if(checkWallCriteria()) 
			finishProcess();
	}

	// check definition of wall and criterias
	private bool checkWallCriteria()
	{
		// wall has more segments than allowed
		if(open.Count >= maxWallTiles)
		{
			Debug.Log("TOO MUCH WALL SEGMENTS!!");
			gui.text = "too much wall segments!";
			building = false;
			automaticMode = false;
			return false;
		}

		// if there are buildings outside the wall 
		for(int i = 0; i < w; ++i)
		{
			for(int j = 0; j < h; ++j)
			{
				if(nodes[i,j].type == 1 && !nodes[i,j].interior)
					return false;
				if(nodes[i,j].d < minWallDistance && !nodes[i,j].interior)
					return false;
			}
		}

		// if all wall segments have only 2 wall segment neighbors
		foreach(Node n in open)
		{
			int wallNeighbors = 0;
			for(int i = -1; i < 2; i++)
			{
				for(int j = -1; j < 2; j++)
				{
					int x = n.posX + i;
					int y = n.posY + j;

					if(i != j && Mathf.Abs(i-j) == 1 && (x >= 0 && x < w && y >= 0 && y < h)) 
					{
						if(open.Contains(nodes[x,y]))
						{
							wallNeighbors ++;
							if(wallNeighbors > 2)
								return false;
							
						}
					}
				}
			}
		}

		// --- at this point, all criteria are met ---

		// if this is the first wall that mets the criteria OR there was a better wall found -> copy openList to openCopy
		if(openCopy.Count == 0 || openCopy.Count > open.Count)
			openCopy = new List<Node>(open);

		// show GUI text
		gui.text = "Mauer mit "+openCopy.Count+" Segmenten gefunden...\nSuche noch "+(futureSteps)+" Schritte um evt. bessere Mauer zu finden.";

		// calculate more steps and find a possible better wall; stop when all future steps were made
		return (--futureSteps <= 0);
	}

	// finish the algorithm and draw the wall
	private void finishProcess()
	{
		building = false;
		automaticMode = false;

		// convert nodes from openlist to wall tiles
		foreach(Node n in openCopy)
			n.tile.changeType(2);

		// unhighlight all tiles
		for(int i = 0; i < w; ++i)
		{
			for(int j = 0; j < h; ++j)
				map.highlightTile(i, j, 0);			
		}

		// show user the statistics
		Debug.Log("FINISHED: "+buildCounter+" steps");
		gui.text = "FINISHED: "+buildCounter+" steps -- WALL SEGMENTS: "+openCopy.Count;  // +", INTERIOR: "+closed.Count;
	}

	// reset the algorithm
	private void reset()
	{
		gui.text = "";
		building = false;
		automaticMode = false;
		resetted = true;

		// unhighlight all tiles and convert walls to grass
		for(int i = 0; i < w; ++i)
		{
			for(int j = 0; j < h; ++j)
			{
				Tile t = map.get(i, j);
				map.highlightTile(i, j, 0);
				if((int)t.type == 2)
					t.changeType(0);
			}
		}

		// clear lists
		open.Clear();
		closed.Clear();
		openCopy.Clear();
	}


	// update the heuristic value of all visited nodes
	private void calculateCosts()
	{
		for(int i = 0; i < w; ++i)
		{
			for(int j = 0; j < h; ++j)
			{
				// calculate distance to start
				int d = distanceToStart(nodes[i,j]);
				nodes[i,j].d = d;

				if(d < minWallDistance || nodes[i,j].type != 0) // natural barrier
				{
					nodes[i,j].c = 0;				// set constant to 0 so the calculated value will be 0
				} 
				else 
				{
					nodes[i,j].c = constantValue;	// set constant value of the node
					wallingCost(nodes[i,j]);		// set walling costs of the node
					nodes[i,j].updateCosts();		// update the heuristic value
				}

			}
		}
	}
	

	// expand the current node
	private void expand(Node n)
	{
		// visit nodes on left and right side of the node
		for(int i = -1; i < 2; i++)
		{
			for(int j = -1; j < 2; j++)
			{
				// index of neighbor node
				int neigX = n.posX + i;
				int neigY = n.posY + j;

				// if index is valid
				if(neigX >= 0 && neigX < w && neigY >= 0 && neigY < h)
				{
					// natural barrier / rock
					if(!nodes[neigX, neigY].listed && nodes[neigX, neigY].type != 3)
					{
						nodes[neigX, neigY].listed = true;		// mark as listed
						open.Add(nodes[neigX, neigY]);			// add to openList
						map.highlightTile(neigX, neigY, 2);		// highlight
					}

					// diagonal tiles
					if(Mathf.Abs(i-j) == 1 && n.interior)
					{
						// add neighbors for tile
						nodes[neigX,neigY].neighbors ++;
						n.neighbors ++;
					}
				}

			}
		}
		//  recalculate costs
		calculateCosts();
	}

	// calculate cost for walling off the node: check for grass tile neighbors (+diagonals)
	private void wallingCost(Node n)
	{
		int cost = 0;
		for(int i = -1; i < 2; ++i)
		{
			for(int j = -1; j < 2; ++j)
			{
				int neigX = n.posX + i;
				int neigY = n.posY + j;
				if(neigX >= 0 && neigX < w && neigY >= 0 && neigY < h)
				{
					if(!nodes[neigX, neigY].listed && nodes[neigX, neigY].type != 3) // not listed or natural barrier
						cost ++;
				}
			}
		}
		// set cost value of  node
		n.w = cost;
	}

	// calculate distance to startnode
	private int distanceToStart(Node n)
	{
		// manhattan distance
		return Mathf.Abs(n.posX - startNode.posX) + Mathf.Abs(n.posY - startNode.posY);
	}

	// convert tile to node
	private Node tile2Node(Tile t)
	{
		// create new node
		Node n = new Node();
		n.type = (int)t.type;

		// add reference to tile
		n.tile = t;

		return n;
	}
}
