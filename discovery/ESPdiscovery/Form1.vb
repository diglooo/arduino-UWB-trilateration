Imports System.Net.Sockets
Imports System.Text
Imports System.Net

Public Class Form1

    Dim UDPClient As UdpClient = New UdpClient(8600)
    IPEndPoint RemoteIpEndPoint = New IPEndPoint(System.Net.IPAddress.Parse("127.0.0.1"), 0)



    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load



    End Sub

End Class
