using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace Karteikarten
{
    public partial class Karte : Form
    {

        private MainForm model;
        private string cardPath;
        public bool deleted;

        public Karte()
        {
            InitializeComponent();
            deleted = false;

            toolDel.Enabled = false;

            cardPath = "";
        }

        public void sendModel(MainForm m)
        {
            model = m;
        }

        private void changeForm()
        {
            string msg = 
                "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//DE\">" 
                + "<html><head><title>Card</title><style type=\"text/css\">"
                + "body { font-family: Tahoma,Verdana,Segoe,sans-serif;  text-align: center;}\n" 
                + "img { max-width: 80%; height: auto;}"
                + "</style></head><body>";

            if (!edtHeader.Text.Equals("") && !edtHeader.Text.Equals("Überschrift"))
            {
                msg += "<h3>"+edtHeader.Text+"</h3>";
            }

            if (!msgText.Text.Equals("Beschreibung"))
            {
                msg += msgText.Text;
            }
            msg += "</body></html>";
            browPrev.DocumentText = msg;

        }

        public void loadCardFromFile(string path, string header, string content)
        {
            cardPath = path;

            edtHeader.Text = header;

            msgText.Text = content;

            toolDel.Enabled = true;

        }

        public string getDocText()
        {
            return browPrev.DocumentText;
        }

        private void txtHeader_Enter(object sender, EventArgs e)
        {
            if (edtHeader.Text.Equals("Überschrift"))
                edtHeader.Text = "";
        }

        private void txtHeader_Leave(object sender, EventArgs e)
        {
            if (edtHeader.Text.Length == 0)
                edtHeader.Text = "Überschrift";
        }

        private void msgText_TextChanged(object sender, EventArgs e)
        {
            changeForm();
        }

        private void msgText_Enter(object sender, EventArgs e)
        {
            if (msgText.Text.Equals("Beschreibung"))
                msgText.Text = "";
        }

        private void msgText_Leave(object sender, EventArgs e)
        {
            if (msgText.Text.Length == 0)
                msgText.Text = "Beschreibung";
        }

        private void button3_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void txtHeader_TextChanged(object sender, EventArgs e)
        {
            changeForm();
        }


        private static string CleanFileName(string fileName)
        {
            return Path.GetInvalidFileNameChars().Aggregate(fileName, (current, c) => current.Replace(c.ToString(), string.Empty));
        }


        private void button2_Click(object sender, EventArgs e)
        {
            if(edtHeader.Text.Equals("") || edtHeader.Text.Equals("Überschrift"))
                MessageBox.Show("Keine Überschrift");
            else if(msgText.Text.Equals("") || msgText.Text.Equals("Beschreibung"))
                MessageBox.Show("Keine Beschreibung");
            else
            {
                string pathString;
                if (cardPath.Equals(""))
                {
                    string cardName = CleanFileName(edtHeader.Text) + ".card";
                    pathString = System.IO.Path.Combine(MainForm.projectDir, cardName);

                    bool breakWhile = false;
                    int counter = 1;
                    while (!breakWhile)
                    {
                        if (System.IO.File.Exists(pathString))
                        {
                            cardName = CleanFileName(edtHeader.Text) + counter + ".card";
                            pathString = System.IO.Path.Combine(MainForm.projectDir, cardName);
                        }
                        else
                            breakWhile = true;

                        counter++;
                        if (counter > 100) breakWhile = true;
                    }
                }
                else
                {
                    pathString = cardPath;
                }
                using (StreamWriter outfile = new StreamWriter(pathString))
                {
                    outfile.Write(browPrev.DocumentText.Replace( MainForm.projectDir, "@PATH"));
                }
                if (model != null)
                    model.readCards();

                Close();
            }           
        }


        private void msgText_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return)
            {
                msgText.SelectedText = "<br>";
            }
        }

   

        private void rückgängigToolStripMenuItem_Click(object sender, EventArgs e)
        {
            msgText.Undo(); 
        }

        private void wiederherstellenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            msgText.Redo();
        }

        private void allesZurücksetzenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            DialogResult res = MessageBox.Show("Alle Daten zurücksetzen?", "Zurücksetzen bestätigen", MessageBoxButtons.YesNo);

            if (res == DialogResult.Yes)
            {
                edtHeader.Text = "";
                msgText.Text = "";

                txtHeader_Leave(this, null);
                msgText_Leave(this, null);

                changeForm();

                edtHeader.Focus();
            }
        }

        private void bIldEinfügenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (dialOpenImg.ShowDialog() == DialogResult.OK)
            {
                System.IO.FileInfo fInfo = new System.IO.FileInfo(dialOpenImg.FileName);
                string targetPath = MainForm.projectDir + "\\images";
                string destFile = System.IO.Path.Combine(targetPath, fInfo.Name);

                if (!System.IO.Directory.Exists(targetPath))
                {
                    System.IO.Directory.CreateDirectory(targetPath);
                }

                bool createFile = true;
                if(System.IO.File.Exists(destFile))
                {  
                    DialogResult res = MessageBox.Show("Datei existiert bereits im Projektordner.\nSoll die Datei ersetzt werden?", "Ersetzen bestätigen", MessageBoxButtons.YesNo);
                    if (res != DialogResult.Yes)
                        createFile = false;
                }
                if (createFile)
                {
                    System.IO.File.Copy(dialOpenImg.FileName, destFile, true);

                    if (msgText.Text.Equals("") || msgText.Text.Equals("Beschreibung"))
                        msgText.Text = "<img src=\"" + MainForm.projectDir + "/images/" + fInfo.Name + "\"><br>\n";
                    else
                        msgText.SelectedText = "<br>\n<img src=\"" + MainForm.projectDir + "/images/" + fInfo.Name + "\"><br>\n";
                    changeForm();
                }
                
            }
        }

        private void audioEinfügenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (dialOpenImg.ShowDialog() == DialogResult.OK)
            {
                System.IO.FileInfo fInfo = new System.IO.FileInfo(dialOpenImg.FileName);
                string targetPath = MainForm.projectDir + "\\audio";
                string destFile = System.IO.Path.Combine(targetPath, fInfo.Name);

                if (!System.IO.Directory.Exists(targetPath))
                {
                    System.IO.Directory.CreateDirectory(targetPath);
                }
                System.IO.File.Copy(dialOpenImg.FileName, destFile, true);
                if (msgText.Text.Equals("") || msgText.Text.Equals("Beschreibung"))
                    msgText.Text = "<embed src=\"" + MainForm.projectDir + "/audio/" + fInfo.Name + "\" width=\"300\" height=\"30\" autostart=false><br>\n";
                else
                    msgText.SelectedText = "<br>\n<embed src=\"" + MainForm.projectDir + "/audio/" + fInfo.Name + "\" width=\"300\" height=\"30\" autostart=false><br>\n";
                changeForm();
            }
        }

        private void löschenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            DialogResult res = MessageBox.Show("Karte wirklich löschen?", "Löschen bestätigen", MessageBoxButtons.YesNo);
            if (res == DialogResult.Yes && !cardPath.Equals(""))
            {
                System.IO.File.Delete(cardPath);
                deleted = true;
                Close();
            }
        }

        private void Karte_Shown(object sender, EventArgs e)
        {
            changeForm();
        }

        private void speichernToolStripMenuItem_Click(object sender, EventArgs e)
        {
            button2_Click(this, null);
        }

        private void abbrechenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        } 

    }
}