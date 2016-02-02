using UnityEngine;
using System.Collections;

public class TileMap : MonoBehaviour {
	
	public enum TileType { grass = 0, building = 1, wall = 2, rock = 3, NULL = -1 };

	//  tilemap width and height
	public int width  = 30;
	public int height = 30;

	//  prefabs for TileObjects; 0:building, 1:wall, 2:rock, 3:grass
	public Transform[] prefs;

	// array for instantiated tiles
	private Transform[,] map;

	// saved type for tilechange with mouthclicks
	private TileType clickType;
	// for buttondown event; when to change clickType
	private bool click;

	// initialization
	void Start () {
		clickType = TileType.NULL;
		click = false;

		map = new Transform[width, height];

		// create array for tile objects
		for(int i = 0; i < width; ++i)
		{
			for(int j = 0; j < height; ++j)
			{
				// use grass prefab for tile model
				map[i, j] = Instantiate(prefs[3], new Vector3(i, 0, j), Quaternion.identity) as Transform;
				map[i, j].gameObject.AddComponent<Tile>();
				map[i, j].GetComponent<Tile>().Init(this, TileType.grass);
			}
		}
	}
	
	// Update is called once per frame
	void Update () {
		// left click; change tile draw type to building
		if( Input.GetMouseButtonDown(0))
		{
			click = true;
			clickType = TileType.building;
		}
		// right click; change tile draw type to rock
		else if(Input.GetMouseButtonDown(1))
		{
			click = true;
			clickType = TileType.rock;
		}

		// release mouse button -> stop drawing tiles on map
		if(Input.GetMouseButtonUp(0) || Input.GetMouseButtonUp(1))
			clickType = TileType.NULL;

		if ( clickType != TileType.NULL ){

			// cast ray and get clicked tile
			Ray ray = Camera.main.ScreenPointToRay (Input.mousePosition);
			RaycastHit hit;

			if (Physics.Raycast(ray, out hit)){
				if(hit.collider.transform.tag == "TILE")
				{
					Tile t = hit.collider.gameObject.GetComponent<Tile>();

					// if mousebutton down event and clicked tile isn't grass -> change draw type to grass
					if(click == true && t.type != TileType.grass)
						clickType = TileType.grass;

					// mouse drag events:
					click = false;
					if(t.type != clickType)
						t.changeType(clickType);
				}
			}
		}
	}
	
	// change tile material color to show how algorithm works
	public void highlightTile(int x, int y, int flag)
	{
		if(flag == 1) 		// closedList
			map[x,y].renderer.material.color = new Color(1f,0.8f,0.8f);
		else if(flag == 2) 	// openList
			map[x,y].renderer.material.color = new Color(0.8f,0.8f,1f);
		else if(flag == 3) 	// openList
			map[x,y].renderer.material.color = new Color(0.8f,1f,0.8f);
		else 				// normal Color
			map[x,y].renderer.material.color = Color.white;
	}

	// get x,y tile
	public Tile get(int x, int y)
	{
		return map[x,y].GetComponent<Tile>();
	}
}
