#include "Message.h"

int main() {
    // 1. Generate Key Pairs for Alice and Bob
    std::cout << "Generating Alice's and Bob's RSA key pairs...\n";
    WinCrypto::PublicKey alicePubKey;
    WinCrypto::PrivateKey alicePrivKey;
    WinCrypto::PublicKey bobPubKey;
    WinCrypto::PrivateKey bobPrivKey;

    try {
        std::tie(alicePubKey, alicePrivKey) = WinCrypto::generateRsaKeyPair();
        std::tie(bobPubKey, bobPrivKey) = WinCrypto::generateRsaKeyPair();
        std::cout << "Key pairs generated successfully.\n";
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Failed to generate key pairs: " << e.what() << std::endl;
        return 1;
    }


    std::cout << "\n--- Alice to Bob Communication ---\n";

    // Alice creates a message for Bob. She needs Bob's Public Key.
    Message aliceMsg(MessageType::GREETING, bobPubKey);
    aliceMsg.pushBody("Hello Bob, this is Alice!");
    aliceMsg.pushBody("How are you doing today?");
    aliceMsg.generateEncryptedMessageForSending(); // This encrypts the body with the symmetric key

    std::cout << "Alice's Message (before sending):\n";
    std::cout << aliceMsg.getFullMessageRawBody() << std::endl; // Shows unencrypted body parts + header info
    std::cout << "Message Alice sends to Bob (encrypted parts as hex):\n";
    std::cout << aliceMsg.getMessageToSend() << std::endl;

    // Bob receives the message. He needs his own Private Key to decrypt it.
    std::cout << "\n--- Bob receives and decrypts Alice's message ---\n";
    std::string receivedData = aliceMsg.getMessageToSend();

    // Bob constructs a Message object using the received data and his private key.
    // The constructor will handle parsing the hex strings and decrypting.
    Message bobReceivedMsg(receivedData, bobPrivKey);

    std::cout << "Bob's Decrypted Message:\n";
    std::cout << "Type: " << messageTypeToString(bobReceivedMsg.type) << "\n";
    std::cout << "Body Parts:\n";
    for (const auto& part : bobReceivedMsg.messageBodyParts) {
        std::cout << "- " << part << "\n";
    }
    std::cout << "------------------------\n";


    std::cout << "\n--- Bob to Alice Communication ---\n";

    // Bob creates a message for Alice. He needs Alice's Public Key.
    Message bobMsg(MessageType::EVENT, alicePubKey);
    bobMsg.pushBody("Hi Alice, Bob here!");
    bobMsg.pushBody("Let's meet tomorrow at 10 AM for the project update.");
    bobMsg.generateEncryptedMessageForSending();

    std::cout << "Message Bob sends to Alice (encrypted parts as hex):\n";
    std::cout << bobMsg.getMessageToSend() << std::endl;

    // Alice receives the message. She needs her own Private Key.
    std::cout << "\n--- Alice receives and decrypts Bob's message ---\n";
    std::string receivedDataFromBob = bobMsg.getMessageToSend();

    // Alice constructs a Message object using the received data and her private key.
    Message aliceReceivedMsg(receivedDataFromBob, alicePrivKey);

    std::cout << "Alice's Decrypted Message:\n";
    std::cout << "Type: " << messageTypeToString(aliceReceivedMsg.type) << "\n";
    std::cout << "Body Parts:\n";
    for (const auto& part : aliceReceivedMsg.messageBodyParts) {
        std::cout << "- " << part << "\n";
    }
    std::cout << "------------------------\n";

    return 0;
}