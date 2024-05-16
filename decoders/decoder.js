// Define the struct fields
const structFields = [
    { name: 'id', size: 1 },
    { name: 'bat_perc', size: 1 },
    { name: 'temp_int', size: 1 },
    { name: 'temp_dec', size: 1 },
    { name: 'humdity_int', size: 1 },
    { name: 'humdity_dec', size: 1 },
    { name: 'bar_press', size: 2 },
    { name: 'inc_x', size: 1 },
    { name: 'inc_y', size: 1 },
    { name: 'inc_z', size: 1 },
    { name: 'iaq', size: 2 },
    { name: 'iaqAccuracy', size: 1 },
    { name: 'co2equivalent', size: 2 },
    { name: 'breathVocEquivalent', size: 2 },
    { name: 'gasPercentage', size: 1 },
    { name: 'sentPackets', size: 2 },
    { name: 'accAlarm', size: 1 },
];

//Function to decode the data
function decodePacket(packet) {
    let offset = 0;
    const decodedData = {};

    structFields.forEach(field => {
        const fieldValue = packet.slice(offset, offset + field.size);
        decodedData[field.name] = fieldValue.readUIntLE(0, field.size);
        offset += field.size;
    });

    return decodedData;
}

// Hex string representing the packet
const hexPacket = "666c160e2e0fc2030000b23200005802000000130000";

// Convert hex string to Buffer
const packetBuffer = Buffer.from(hexPacket, 'hex');

// Decode the packet
const decodedData = decodePacket(packetBuffer);

console.log(decodedData);