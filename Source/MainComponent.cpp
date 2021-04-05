

#include "MainComponent.h"
#include "ScoreWindow.h"
#include "Board.h"
#include "Queue.h"
#include "Randomizer.h"

static constexpr int TILESIZE = 70;
static constexpr int HALFTILE = TILESIZE / 2;

static constexpr int BOARD_VSTARTPOS = 80;
static constexpr int BOARD_HSTARTPOS = 175;

static constexpr int MAX_NUM_BOMBS = 5;


MainComponent::MainComponent()
	:	m_blockInteraction(0),
		m_difficultyLevel(1),
		m_cummulativeScore(0),
		m_scoreWindow(nullptr),
		m_numBombs(MAX_NUM_BOMBS)
{
    setSize (900, 600);

	// Create board
	m_board = new Board(10, 7);

	// Create queue
	m_queue = new Queue(5);

	// Initialize randomizer and store pointer
	//  to ensure it is deleted on shutdown.
	m_randomizer = Randomizer::GetInstance();

	// GUI-refreh rate
	startTimer(60);
}

MainComponent::~MainComponent()
{
	delete m_board;
	delete m_queue;
	delete m_scoreWindow;
	delete m_randomizer;
}

void MainComponent::paint(juce::Graphics& g)
{
	const juce::ScopedLock lock(m_lock);

    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::Font (16.0f));
    g.setColour (juce::Colours::white);
    //g.drawText ("Hello World!", getLocalBounds(), juce::Justification::centred, true);

	// Draw bombs
	DrawBombs(juce::Point<int>(538, 20), g);

	// Draw tile queue
	static constexpr int queueVStartPos = 450;
	static constexpr int queueHStartPos = 50;
	for (int i = 0; i < m_queue->GetSize(); i++)
	{
		// Pipe shape
		juce::Point<int> p(queueHStartPos, queueVStartPos - i * (TILESIZE - 0));
		DrawTile((m_queue->GetTile(i)), p, g);

		if (i == 0)
		{
			// Extra frame for the tile at the start of the queue.
			g.setColour(juce::Colours::red);
			g.drawRect(queueHStartPos, queueVStartPos - i * (TILESIZE - 0), TILESIZE, TILESIZE, 4);
		}
	}

	// Draw board.
	for (int i = 0; i < m_board->GetNumCols(); i++)
	{
		for (int j = 0; j < m_board->GetNumRows(); j++)
		{
			juce::Point<int> p(BOARD_HSTARTPOS + i * (TILESIZE - 1), BOARD_VSTARTPOS + j * (TILESIZE - 1));

			DrawTile(m_board->GetTile(i, j), p, g);
			DrawOoze(m_board->GetTile(i, j), p, g);
		}
	}
}

void MainComponent::resized()
{
    // This is called when the MainComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}

void MainComponent::timerCallback()
{
	// When it reaches 0, clicks are enabled again.
	if (m_blockInteraction > 0)
		m_blockInteraction--;

	// Pump more ooze into the board!
	bool ok = m_board->Pump(GetCurrentOozePerPump());
	if (!ok)
	{
		// Ooze spill! This round if over, show scoreboard.
		stopTimer();

		int score = m_board->GetScore();

		// Show scoreboard overlay.
		m_scoreWindow = new ScoreWindow(score);
		m_scoreWindow->addChangeListener(this);
		addAndMakeVisible(m_scoreWindow);
		m_scoreWindow->setBounds(juce::Rectangle<int>(320, 180, 400, 300));
	}

	this->repaint();
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
	const juce::ScopedLock lock(m_lock);

	if (m_blockInteraction == 0)
	{
		juce::Point<int> clickPos = event.getMouseDownPosition();

		for (int i = 0; i < m_board->GetNumCols(); i++)
		{
			for (int j = 0; j < m_board->GetNumRows(); j++)
			{
				juce::Rectangle<int> tileRect(	BOARD_HSTARTPOS + i * (TILESIZE - 1), 
												BOARD_VSTARTPOS + j * (TILESIZE - 1), 
												TILESIZE, TILESIZE);
				if (tileRect.contains(clickPos))
				{
					static constexpr int framesInteractionBlocked = 5;

					TilePiece* clickedTile = m_board->GetTile(i, j);
					bool replace(clickedTile->GetType() == TilePiece::TYPE_NONE);

					if (!replace)
					{
						Pipe* clickedPipe = dynamic_cast<Pipe*>(clickedTile);
						if ((clickedPipe != nullptr) &&
							(clickedPipe->IsEmpty()) &&		// Only empty tiles can be replaced.
							(!clickedPipe->IsStart()) &&	// Cannot replace starter tiles.
							(m_numBombs > 0))				// Need bombs to replace existing pipe tiles.
						{
							replace = true;

							// One bomb was used up.
							m_numBombs--;

							// Using bombs disables actions for a few more frames.
							m_blockInteraction += framesInteractionBlocked;
						}
					}

					if (replace)
					{
						// Grab the next piece in the queue, and...
						TilePiece::Type newType = m_queue->Pop();

						// ... place it on the board.
						m_board->ReplaceTile(i, j, newType);

						// To prevent accidental double-clicking disable actions for a few frames.
						m_blockInteraction += framesInteractionBlocked;
					}
				}
			}
		}
	}
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source) 
{
	(void)source;

	//ScoreWindow::Command cmd(ScoreWindow::CMD_QUIT);
	if ((m_scoreWindow != nullptr) &&
		(m_scoreWindow->GetCommand() == ScoreWindow::CMD_CONTINUE))
	{
		m_board->Reset();
		m_queue->Reset();
		m_blockInteraction = 0;
		m_numBombs = MAX_NUM_BOMBS;

		// TODO find out if we levelled up
		m_difficultyLevel += 1;

		// GUI-refreh rate
		startTimer(60);
	}

	delete m_scoreWindow;
	m_scoreWindow = nullptr;
}

juce::Colour MainComponent::GetCurrentTileColor() const
{
	static const juce::Colour colorsPerLevel[] = {
		juce::Colours::grey,		   // Level 1
		juce::Colours::cadetblue,	   // Level 2
		juce::Colours::darkkhaki,	   // Level 3
		juce::Colours::darkolivegreen, // Level 4
		juce::Colours::darkslategrey,  // Level 5
		juce::Colours::rosybrown,	   // Level 6
		juce::Colours::dimgrey,		   // Level 7
		juce::Colours::lightslategrey, // Level 8
		juce::Colours::lightsteelblue, // Level 9
		juce::Colours::olivedrab,	   // Level 10
		juce::Colours::slategrey,	   // Level 11
		juce::Colours::pink,		   // Level 12
	};

	// m_difficultyLevel starts at 1
	int arraySize = sizeof(colorsPerLevel) / sizeof(*colorsPerLevel);
	int level = m_difficultyLevel - 1;
	if (level >= arraySize)
		level = arraySize - 1;

	return colorsPerLevel[level];
}

float MainComponent::GetCurrentOozePerPump() const
{
	static const float oozePerLevel[] = { 
		0.6F, // Level 1
		0.7F, // Level 2
		0.8F, // Level 3
		0.9F, // Level 4
		1.0F, // Level 5
		1.1F, // Level 6
		1.2F, // Level 7
		1.3F, // Level 8
		1.4F, // Level 9
		1.5F, // Level 10
		1.6F, // Level 11
		1.7F  // Level 12
	};

	// m_difficultyLevel starts at 1
	int arraySize = sizeof(oozePerLevel) / sizeof(*oozePerLevel);
	int level = m_difficultyLevel - 1;
	if (level >= arraySize)
		level = arraySize - 1;

	return oozePerLevel[level];
}

void MainComponent::DrawTile(TilePiece* tile, juce::Point<int> origin, juce::Graphics& g)
{
	static constexpr float pipeThickness = 20.0f;

	if (tile->GetType() != TilePiece::TYPE_NONE)
	{
		Pipe* pipe = dynamic_cast<Pipe*>(tile);
		if (pipe != nullptr)
		{
			// Draw tile's Background color
			g.setColour(GetCurrentTileColor());
			g.fillRect(origin.getX(), origin.getY(), TILESIZE, TILESIZE);

			juce::Line<int> line;
			g.setColour(juce::Colours::black);

			switch (pipe->GetType())
			{
			case TilePiece::TYPE_START_N:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY(),
											origin.getX() + HALFTILE,
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_START_S:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY() + HALFTILE,
											origin.getX() + HALFTILE,
											origin.getY() + TILESIZE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_START_E:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE, 
											origin.getY() + HALFTILE, 
											origin.getX() + TILESIZE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_START_W:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY() + HALFTILE,
											origin.getX(), 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_VERTICAL:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY(), 
											origin.getX() + HALFTILE,
											origin.getY() + TILESIZE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_HORIZONTAL:
				{
					line = juce::Line<int>(	origin.getX(), 
											origin.getY() + HALFTILE,
											origin.getX() + TILESIZE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_NW_ELBOW:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY(), 
											origin.getX() + HALFTILE,
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
					line = juce::Line<int>(	origin.getX(), 
											origin.getY() + HALFTILE,
											origin.getX() + HALFTILE,
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_NE_ELBOW:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY(), 
											origin.getX() + HALFTILE,
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
					line = juce::Line<int>(	origin.getX() + HALFTILE, 
											origin.getY() + HALFTILE, 
											origin.getX() + TILESIZE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_SE_ELBOW:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY() + HALFTILE,
											origin.getX() + HALFTILE,
											origin.getY() + TILESIZE);
					g.drawLine(line.toFloat(), pipeThickness);
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY() + HALFTILE,
											origin.getX() + TILESIZE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_SW_ELBOW:
				{
					line = juce::Line<int>(	origin.getX() + HALFTILE,
											origin.getY() + HALFTILE,
											origin.getX() + HALFTILE,
											origin.getY() + TILESIZE);
					g.drawLine(line.toFloat(), pipeThickness);
					line = juce::Line<int>(	origin.getX(), 
											origin.getY() + HALFTILE, 
											origin.getX() + HALFTILE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			case TilePiece::TYPE_CROSS:
				{
					line = juce::Line<int>(	origin.getX(), 
											origin.getY() + HALFTILE, 
											origin.getX() + TILESIZE, 
											origin.getY() + HALFTILE);
					g.drawLine(line.toFloat(), pipeThickness);
					line = juce::Line<int>(	origin.getX() + HALFTILE, 
											origin.getY(), 
											origin.getX() + HALFTILE, 
											origin.getY() + TILESIZE);
					g.drawLine(line.toFloat(), pipeThickness);
				}
				break;

			default:
				break;
			}

			// Frame
			g.setColour(juce::Colours::white);
			g.drawRect(origin.getX(), origin.getY(), TILESIZE, TILESIZE, 1);

			// If this tile has an explosion on it, draw it.
			int exp = pipe->PopExplosion();
			if (exp > 0)
			{
				juce::Path starPath;
				starPath.addStar(juce::Point<float>(static_cast<float>(	origin.getX() + HALFTILE),
																		static_cast<float>(origin.getY() + HALFTILE)),
																		7,								// Number of peaks
																		static_cast<float>(exp * 4),	// Inner radius
																		static_cast<float>(exp * 8),	// Outer radius
																		static_cast<float>(exp * 2));	// Rotation angle
				g.setColour(juce::Colours::orangered);
				g.fillPath(starPath);
			}
		}
	}

	// Empty tile
	else
	{
		// Frame
		g.setColour(juce::Colours::white);
		g.drawRect(origin.getX(), origin.getY(), TILESIZE, TILESIZE, 1);
	}
}

void MainComponent::DrawOoze(TilePiece* tile, juce::Point<int> origin, juce::Graphics& g)
{
	if (tile->GetType() != TilePiece::TYPE_NONE)
	{
		Pipe* pipe = dynamic_cast<Pipe*>(tile);
		if ((pipe != nullptr) &&
			(!pipe->IsEmpty()))
		{
			static constexpr float oozeThickness = 15.0f;
			bool overHalf(pipe->GetOozeLevel() >= 50.0f);
			bool underHalf(pipe->GetOozeLevel() < 50.0f);
			int fill = static_cast<int>(TILESIZE * pipe->GetOozeLevel() / MAX_OOZE_LEVEL);

			juce::Line<int> line;
			g.setColour(juce::Colours::limegreen);

			switch (pipe->GetType())
			{
			case TilePiece::TYPE_START_N:
				{
					if (overHalf)
					{
						line = juce::Line<int>(	origin.getX() + HALFTILE, 
												origin.getY() + TILESIZE - fill,
												origin.getX() + HALFTILE,
												origin.getY() + HALFTILE);
						g.drawLine(line.toFloat(), oozeThickness);
					}
				}
				break;

			case TilePiece::TYPE_START_S:
				{
					if (overHalf)
					{
						line = juce::Line<int>(	origin.getX() + HALFTILE, 
												origin.getY() + HALFTILE,
												origin.getX() + HALFTILE,
												origin.getY() + fill);
						g.drawLine(line.toFloat(), oozeThickness);
					}
				}
				break;

			case TilePiece::TYPE_START_E:
				{
					if (overHalf)
					{
						line = juce::Line<int>(	origin.getX() + HALFTILE, 
												origin.getY() + HALFTILE, 
												origin.getX() + fill, 
												origin.getY() + HALFTILE);
						g.drawLine(line.toFloat(), oozeThickness);
					}
				}
				break;

			case TilePiece::TYPE_START_W:
				{
					if (overHalf)
					{
						line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
												origin.getY() + HALFTILE,
												origin.getX() + HALFTILE,
												origin.getY() + HALFTILE);
						g.drawLine(line.toFloat(), oozeThickness);
					}
				}
				break;

			case TilePiece::TYPE_VERTICAL:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_N)
						line = juce::Line<int>(	origin.getX() + HALFTILE, 
												origin.getY() + TILESIZE - fill, 
												origin.getX() + HALFTILE, 
												origin.getY() + TILESIZE);
					else if (pipe->GetFlowDirection() == Pipe::DIR_S)
						line = juce::Line<int>(	origin.getX() + HALFTILE, 
												origin.getY(), 
												origin.getX() + HALFTILE, 
												origin.getY() + fill);

					g.drawLine(line.toFloat(), oozeThickness);
				}
				break;

			case TilePiece::TYPE_HORIZONTAL:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_E)
						line = juce::Line<int>(	origin.getX(), 
												origin.getY() + HALFTILE, 
												origin.getX() + fill, 
												origin.getY() + HALFTILE);
					else if (pipe->GetFlowDirection() == Pipe::DIR_W)
						line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
												origin.getY() + HALFTILE, 
												origin.getX() + TILESIZE, 
												origin.getY() + HALFTILE);

					g.drawLine(line.toFloat(), oozeThickness);
				}
				break;

			case TilePiece::TYPE_NW_ELBOW:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_N)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX(), 
													origin.getY() + HALFTILE, 
													origin.getX() + fill, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX(), 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE - fill, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
					else if (pipe->GetFlowDirection() == Pipe::DIR_W)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY(), 
													origin.getX() + HALFTILE, 
													origin.getY() + fill);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY(), 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
				}
				break;

			case TilePiece::TYPE_NE_ELBOW:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_N)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
													origin.getY() + HALFTILE, 
													origin.getX() + TILESIZE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + TILESIZE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE - fill, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
					else if (pipe->GetFlowDirection() == Pipe::DIR_E)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY(), 
													origin.getX() + HALFTILE, 
													origin.getY() + fill);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY(), 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + fill, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
				}
				break;

			case TilePiece::TYPE_SE_ELBOW:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_S)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
													origin.getY() + HALFTILE, 
													origin.getX() + TILESIZE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + TILESIZE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + fill);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
					else if (pipe->GetFlowDirection() == Pipe::DIR_E)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE - fill, 
													origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + fill, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
				}
				break;

			case TilePiece::TYPE_SW_ELBOW:
				{
					if (pipe->GetFlowDirection() == Pipe::DIR_S)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX(), 
													origin.getY() + HALFTILE, 
													origin.getX() + fill, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX(), 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + fill);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
					else if (pipe->GetFlowDirection() == Pipe::DIR_W)
					{
						if (underHalf)
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE - fill, 
													origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + TILESIZE);
							g.drawLine(line.toFloat(), oozeThickness);
							line = juce::Line<int>(	origin.getX() + TILESIZE - fill, 
													origin.getY() + HALFTILE, 
													origin.getX() + HALFTILE, 
													origin.getY() + HALFTILE);
							g.drawLine(line.toFloat(), oozeThickness);
						}
					}
				}
				break;

			case TilePiece::TYPE_CROSS:
				{
					Cross* crossTile = dynamic_cast<Cross*>(pipe);

					fill = static_cast<int>(TILESIZE * crossTile->GetOozeLevel(Cross::WAY_VERTICAL) / MAX_OOZE_LEVEL);
					if (fill > 0)
					{
						if (pipe->GetFlowDirection() == Pipe::DIR_N)
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE,
													origin.getY() + TILESIZE - fill,
													origin.getX() + HALFTILE,
													origin.getY() + TILESIZE);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + HALFTILE,
													origin.getY(),
													origin.getX() + HALFTILE,
													origin.getY() + fill);
						}

						g.drawLine(line.toFloat(), oozeThickness);
					}

					fill = static_cast<int>(TILESIZE * crossTile->GetOozeLevel(Cross::WAY_HORIZONTAL) / MAX_OOZE_LEVEL);
					if (fill > 0)
					{
						if (pipe->GetFlowDirection() == Pipe::DIR_E)
						{
							line = juce::Line<int>(	origin.getX(),
													origin.getY() + HALFTILE,
													origin.getX() + fill,
													origin.getY() + HALFTILE);
						}
						else
						{
							line = juce::Line<int>(	origin.getX() + TILESIZE - fill,
													origin.getY() + HALFTILE,
													origin.getX() + TILESIZE,
													origin.getY() + HALFTILE);
						}

						g.drawLine(line.toFloat(), oozeThickness);
					}

				}
				break;

			default:
				break;
			}
		}
	}
}

void MainComponent::DrawBombs(juce::Point<int> p, juce::Graphics& g)
{
	for (int i = 0; i < MAX_NUM_BOMBS; i++)
	{
		if (i < m_numBombs)
		{
			g.setColour(juce::Colours::red);
			g.fillEllipse(juce::Rectangle<int>(p.getX() + i * (TILESIZE - 1), p.getY(), HALFTILE, HALFTILE).toFloat());
		}

		g.setColour(juce::Colours::white);
		g.drawEllipse(juce::Rectangle<int>(p.getX() + i * (TILESIZE - 1), p.getY(), HALFTILE, HALFTILE).toFloat(), 1.0f);
	}
}
